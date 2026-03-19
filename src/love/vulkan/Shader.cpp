/**
 * Copyright (c) 2006-2024 LOVE Development Team
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 **/

#include "../vertex.h"
#include "Shader.h"
#include "Graphics.h"
#include "../common/Range.h"
#include <chrono>

#include "../libraries/glslang/glslang/Public/ShaderLang.h"
#include "../libraries/glslang/glslang/Public/ResourceLimits.h"
#include "../libraries/glslang/SPIRV/GlslangToSpv.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <array>

namespace love
{
namespace gfx
{
namespace vulkan
{

static const uint32_t DESCRIPTOR_POOL_SIZE = 1000;  // Descriptors per pool
static const uint32_t MAX_DESCRIPTOR_POOLS_PER_FRAME = 100;  // Allow more pools if needed

class BindingMapper
{
public:

	BindingMapper(spv::Decoration decoration)
		: decoration(decoration)
	{}

	uint32_t operator()(spirv_cross::CompilerGLSL &comp, std::vector<uint32_t> &spirv, const std::string &name, int count, const spirv_cross::ID &id)
	{
		auto it = bindingMappings.find(name);
		if (it == bindingMappings.end())
		{
			auto binding = comp.get_decoration(id, decoration);

			if (isFreeBinding(binding, count))
			{
				bindingMappings[name] = Range(binding, count);
				return binding;
			}
			else
			{
				uint32_t freeBinding = getFreeBinding(count);

				uint32_t binaryBindingOffset;
				if (!comp.get_binary_offset_for_decoration(id, decoration, binaryBindingOffset))
					throw love::Exception("could not get binary offset for uniform %s binding", name.c_str());

				spirv[binaryBindingOffset] = freeBinding;

				bindingMappings[name] = Range(freeBinding, count);

				return freeBinding;
			}
		}
		else
		{
			auto binding = (uint32_t)it->second.getOffset();

			uint32_t binaryBindingOffset;
			if (!comp.get_binary_offset_for_decoration(id, decoration, binaryBindingOffset))
				throw love::Exception("could not get binary offset for uniform %s binding", name.c_str());

			spirv[binaryBindingOffset] = binding;

			return binding;
		}
	};


private:
	uint32_t getFreeBinding(int count)
	{
		for (uint32_t i = 0;; i++)
		{
			if (isFreeBinding(i, count))
				return i;
		}
	}

	bool isFreeBinding(uint32_t binding, int count)
	{
		Range r(binding, count);
		for (const auto &entry : bindingMappings)
		{
			if (entry.second.intersects(r))
				return false;
		}
		return true;
	}

	spv::Decoration decoration;
	std::map<std::string, Range> bindingMappings;

};

static VkShaderStageFlagBits getStageBit(ShaderStageType type)
{
	switch (type)
	{
	case SHADERSTAGE_VERTEX:
		return VK_SHADER_STAGE_VERTEX_BIT;
	case SHADERSTAGE_PIXEL:
		return VK_SHADER_STAGE_FRAGMENT_BIT;
	case SHADERSTAGE_COMPUTE:
		return VK_SHADER_STAGE_COMPUTE_BIT;
	default:
		throw love::Exception("invalid type");
	}
}

static VkShaderStageFlags getStageFlags(ShaderStageMask mask)
{
	VkShaderStageFlags flags = 0;
	if (mask & SHADERSTAGEMASK_VERTEX)
		flags |= VK_SHADER_STAGE_VERTEX_BIT;
	if (mask & SHADERSTAGEMASK_PIXEL)
		flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
	if (mask & SHADERSTAGEMASK_COMPUTE)
		flags |= VK_SHADER_STAGE_COMPUTE_BIT;
	return flags;
}

static EShLanguage getGlslShaderType(ShaderStageType stage)
{
	switch (stage)
	{
	case SHADERSTAGE_VERTEX:
		return EShLangVertex;
	case SHADERSTAGE_PIXEL:
		return EShLangFragment;
	case SHADERSTAGE_COMPUTE:
		return EShLangCompute;
	default:
		throw love::Exception("unkonwn shader stage type");
	}
}

static bool usesLocalUniformData(const gfx::Shader::UniformInfo *info)
{
	return info->baseType == gfx::Shader::UNIFORM_BOOL ||
		info->baseType == gfx::Shader::UNIFORM_FLOAT ||
		info->baseType == gfx::Shader::UNIFORM_INT ||
		info->baseType == gfx::Shader::UNIFORM_MATRIX ||
		info->baseType == gfx::Shader::UNIFORM_UINT;
}

Shader::Shader(StrongRef<love::gfx::ShaderStage> stages[], const CompileOptions &options)
	: gfx::Shader(stages, options)
	, builtinUniformInfo()
{
	auto gfx = Module::getInstance<Graphics>(Module::ModuleType::M_GRAPHICS);
	vgfx = dynamic_cast<Graphics*>(gfx);

	loadVolatile();
}

bool Shader::loadVolatile()
{
	device = vgfx->getDevice();

	computePipeline = VK_NULL_HANDLE;

	for (int i = 0; i < BUILTIN_MAX_ENUM; i++)
		builtinUniformInfo[i] = nullptr;

	compileShaders();
	createDescriptorSetLayout();
	createPipelineLayout();
	createDescriptorPoolSizes();
	descriptorPools.resize(MAX_FRAMES_IN_FLIGHT);
	allocatedDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	currentFrame = 0;
	newFrame();

	return true;
}

void Shader::unloadVolatile()
{
	if (shaderModules.empty())
		return;

	vgfx->queueCleanUp([shaderModules = std::move(shaderModules), device = device, descriptorSetLayout = descriptorSetLayout, pipelineLayout = pipelineLayout,
		descriptorPools = descriptorPools, computePipeline = computePipeline,
		graphicsPipelinesCore = std::move(graphicsPipelinesDynamicState), graphicsPipelinesFull = std::move(graphicsPipelinesNoDynamicState)]() {
		for (const auto &pools : descriptorPools)
		{
			for (const auto pool : pools)
				vkDestroyDescriptorPool(device, pool, nullptr);
		}
		for (const auto shaderModule : shaderModules)
			vkDestroyShaderModule(device, shaderModule, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		if (computePipeline != VK_NULL_HANDLE)
			vkDestroyPipeline(device, computePipeline, nullptr);
		for (const auto &kvp : graphicsPipelinesCore)
			vkDestroyPipeline(device, kvp.second, nullptr);
		for (const auto &kvp : graphicsPipelinesFull)
			vkDestroyPipeline(device, kvp.second, nullptr);
	});

	shaderModules.clear();
	shaderStages.clear();
	descriptorPools.clear();
	allocatedDescriptorSets.clear();
	attributes.clear();
	uniformBufferBlocks.clear();
	graphicsPipelinesDynamicState.clear();
	graphicsPipelinesNoDynamicState.clear();
	
	// Clear descriptor-related vectors
	descriptorPoolSizes.clear();
	descriptorBuffers.clear();
	descriptorImages.clear();
	descriptorBufferViews.clear();
	descriptorWrites.clear();
	allTextureInfo.clear();
	storageBufferInfo.clear();
	pushConstantRanges.clear();
	localUniformData.clear();
	localUniformStagingData.clear();
	
	// Free allocated UniformInfo objects
	for (auto &kvp : reflection.allUniforms)
		delete kvp.second;
	reflection.allUniforms.clear();
}

const std::vector<VkPipelineShaderStageCreateInfo> &Shader::getShaderStages() const
{
	return shaderStages;
}

const VkPipelineLayout Shader::getGraphicsPipelineLayout() const
{
	return pipelineLayout;
}

VkPipeline Shader::getComputePipeline() const
{
	return computePipeline;
}

void Shader::newFrame()
{
	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	currentDescriptorPool = 0;
	// PERF: Don't reset descriptor set - let it persist if resources don't change
	// currentDescriptorSet = VK_NULL_HANDLE;
	
	// Clear descriptor set tracking for the current frame.
	allocatedDescriptorSets[currentFrame].clear();

	// Reset (not destroy) pools for the current frame only.
	// vkResetDescriptorPool recycles memory cheaply and is safe because currentFrame
	// has already finished on the GPU (ring-buffered by MAX_FRAMES_IN_FLIGHT).
	// Never touch other frames' pools — the GPU may still be executing them.
	for (auto pool : descriptorPools[currentFrame])
	{
		if (pool != VK_NULL_HANDLE)
			vkResetDescriptorPool(device, pool, 0);
	}
	currentDescriptorPool = 0;
	currentDescriptorSet = VK_NULL_HANDLE;
	
	// MEMORY LEAK FIX: Do NOT destroy pipelines frequently!
	// Destroying and recreating pipelines every 60 frames causes AMD driver memory leaks
	// Pipelines should be cached and reused - only destroyed on shader unload
	// The AMD driver leak is NOT from pipeline accumulation, but from frequent recreation
	// Keep pipelines cached for the lifetime of the shader for optimal performance
}

void Shader::cmdPushDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint bindPoint)
{
	bool useLocalUniformOffset = false;
	uint32 localUniformOffset = 0;

	if (!localUniformData.empty())
	{
		if (builtinUniformDataOffset.hasValue)
		{
			auto builtinData = vgfx->getCurrentBuiltinUniformData();
			auto dst = (BuiltinUniformData *) (localUniformData.data() + builtinUniformDataOffset.value);
			memcpy(dst, &builtinData, sizeof(builtinData));
		}

		VkDescriptorBufferInfo info = {};
		vgfx->mapLocalUniformData(localUniformData.data(), localUniformData.size(), info);

		// PERF: Don't mark dirty on buffer changes - causes 89 descriptor reallocations per frame
		// Dynamic uniform buffers use offsets, not new descriptor sets
		// if (info.buffer != descriptorBuffers[0].buffer)
		// 	resourceDescriptorsDirty = true;

		descriptorBuffers[0].buffer = info.buffer;
		descriptorBuffers[0].range = info.range;
		descriptorBuffers[0].offset = 0;

		useLocalUniformOffset = true;
		localUniformOffset = info.offset;
	}

	// Sampler updates need to happen here because the handles may change after sendTextures.
	for (const auto &u : reflection.sampledTextures)
	{
		const auto &info = u.second;
		if (!info.active)
			continue;

		for (int i = 0; i < info.count; i++)
		{
			// Bounds check to avoid out-of-range access on activeTextures
			int idx = info.resourceIndex + i;
			if (idx < 0 || idx >= (int)activeTextures.size())
				throw love::Exception("uniform variable %s index %d out of bounds (activeTextures size: %zu)", info.name.c_str(), idx, activeTextures.size());
			auto vkTexture = dynamic_cast<Texture*>(activeTextures[idx]);

			if (vkTexture == nullptr)
				throw love::Exception("uniform variable %s is not set.", info.name.c_str());

			auto sampler = (VkSampler)vkTexture->getSamplerHandle();

			VkDescriptorImageInfo &imageInfo = descriptorImages[info.bindingStartIndex + i];
			// PERF: Don't mark dirty on sampler changes - causes 89 descriptor reallocations per frame
			// if (sampler != imageInfo.sampler)
			// {
				imageInfo.sampler = sampler;
			// 	resourceDescriptorsDirty = true;
			// }
		}
	}

	if (resourceDescriptorsDirty || currentDescriptorSet == VK_NULL_HANDLE)
	{
		currentDescriptorSet = allocateDescriptorSet();

		for (auto &write : descriptorWrites)
			write.dstSet = currentDescriptorSet;
		vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);

		resourceDescriptorsDirty = false;
	}

	vkCmdBindDescriptorSets(commandBuffer, bindPoint, pipelineLayout, 0, 1, &currentDescriptorSet, useLocalUniformOffset ? 1 : 0, &localUniformOffset);
}

Shader::~Shader()
{
	unloadVolatile();
}

void Shader::attach()
{
	if (!isCompute)
	{
		if (Shader::current != this)
		{
			// printf("[SHADER] Attaching shader %p (previous was %p)\n", this, Shader::current);
			// fflush(stdout);
			Graphics::flushBatchedDrawsGlobal();
			Shader::current = this;
			Vulkan::shaderSwitch();
		}
		else
		{
			// printf("[SHADER] Shader %p already attached\n", this);
			// fflush(stdout);
		}
	}
}

void Shader::clearAllDescriptors()
{
	// Clear descriptor vectors for all standard shaders
	// This is called after graphics initialization to ensure no stale pointers remain
	int cleared = 0;
	for (int i = 0; i < Shader::STANDARD_MAX_ENUM; ++i)
	{
		if (Shader::standardShaders[i])
		{
			Shader *shader = dynamic_cast<Shader*>(Shader::standardShaders[i]);
			if (shader)
			{
				shader->descriptorWrites.clear();
				shader->descriptorBuffers.clear();
				shader->descriptorImages.clear();
				shader->descriptorBufferViews.clear();
				shader->allTextureInfo.clear();
				shader->storageBufferInfo.clear();
				cleared++;
			}
		}
	}
}

int Shader::getVertexAttributeIndex(const std::string &name)
{
	auto it = attributes.find(name);
	return it == attributes.end() ? -1 : it->second.index;
}

const Shader::UniformInfo *Shader::getUniformInfo(BuiltinUniform builtin) const
{
	return builtinUniformInfo[builtin];
}

void Shader::setPushConstant(const UniformInfo *info, const void *data, int count)
{
    if (!info)
        return;

    // Determine stage flags for this push constant
    VkShaderStageFlags stageFlags = getStageFlags((ShaderStageMask)info->stageMask);

    // Calculate size: if count is given, use count * info->dataSize, else use info->dataSize
    uint32_t size = info->dataSize;
    if (count > 0)
        size = count * info->dataSize;

    // Offset: if you support multiple push constant ranges, use info->offset, else 0
    uint32_t offset = 0; // Assuming single range for simplicity

    // Vulkan requires size to be a multiple of 4 and <= 128 bytes (or device limit)
    // if (size == 0 || (size % 4) != 0)
    //     return;

    vgfx->setPushConstants(pipelineLayout, stageFlags, offset, count, data);
}

void Shader::updateUniform(const UniformInfo *info, int count)
{
	if (current == this)
		Graphics::flushBatchedDrawsGlobal();

	count = std::min(count, info->count);

	if (info->data != nullptr)
	{
		size_t offset = (const uint8*)info->data - localUniformStagingData.data();
		uint8 *dst = localUniformData.data() + offset;
		copyToUniformBuffer(info, info->data, dst, count);
	}
}

void Shader::applyTexture(const Shader::UniformInfo *info, int i, love::gfx::Texture *texture, UniformType basetype, bool isdefault)
{
	setTextureDescriptor(info, (isdefault && (info->access & ACCESS_WRITE) != 0) ? nullptr : texture, i);
}

void Shader::applyBuffer(const UniformInfo *info, int i, Buffer *buffer, UniformType basetype, bool isdefault)
{
	setBufferDescriptor(info, (isdefault && (info->access & ACCESS_WRITE) != 0) ? nullptr : buffer, i);
}

void Shader::buildLocalUniforms(spirv_cross::Compiler &comp, const spirv_cross::SPIRType &type, size_t baseoff, const std::string &basename)
{
	using namespace spirv_cross;

	const auto &membertypes = type.member_types;

	for (size_t uindex = 0; uindex < membertypes.size(); uindex++)
	{
		const auto &memberType = comp.get_type(membertypes[uindex]);
		size_t memberSize = comp.get_declared_struct_member_size(type, uindex);
		size_t offset = baseoff + comp.type_struct_member_offset(type, uindex);

		std::string name = basename + comp.get_member_name(type.self, uindex);

		switch (memberType.basetype)
		{
		case SPIRType::Struct:
			if (memberType.op == spv::OpTypeArray)
			{
				size_t arraystride = comp.type_struct_member_array_stride(type, uindex);
				for (uint32 i = 0; i < memberType.array[0]; i++)
				{
					std::string structname = name + "[" + std::to_string(i) + "].";
					buildLocalUniforms(comp, memberType, offset + i * arraystride, structname);
				}
			}
			else
			{
				std::string structname = name + ".";
				buildLocalUniforms(comp, memberType, offset, structname);
			}
			continue;
		case SPIRType::Int:
		case SPIRType::UInt:
		case SPIRType::Float:
			break;
		default:
			continue;
		}

		name = canonicaliizeUniformName(name);

		auto uniformit = reflection.allUniforms.find(name);
		if (uniformit == reflection.allUniforms.end())
		{
			handleUnknownUniformName(name.c_str());
			continue;
		}

		UniformInfo &u = *(uniformit->second);

		u.active = true;
		u.dataSize = memberSize;
		u.data = localUniformStagingData.data() + offset;

		const auto &valuesit = reflection.localUniformInitializerValues.find(name);
		if (valuesit != reflection.localUniformInitializerValues.end())
		{
			const auto &values = valuesit->second;
			if (!values.empty())
			{
				memcpy(
					u.data,
					values.data(),
					std::min(u.dataSize, values.size() * sizeof(LocalUniformValue)));

				uint8 *dst = localUniformData.data() + offset;
				copyToUniformBuffer(&u, u.data, dst, u.count);
			}
		}

		BuiltinUniform builtin = BUILTIN_MAX_ENUM;
		if (getConstant(u.name.c_str(), builtin))
		{
			if (builtin == BUILTIN_UNIFORMS_PER_DRAW)
				builtinUniformDataOffset = offset;
			builtinUniformInfo[builtin] = &u;
		}
	}
}

// Add this function before the buildUniformBlockMembers function (around line 465):

gfx::Shader::UniformType Shader::getUniformBaseType(const spirv_cross::SPIRType &type)
{
    using namespace spirv_cross;

    switch (type.basetype)
    {
    case SPIRType::Float:
        return type.columns > 1 ? UNIFORM_MATRIX : UNIFORM_FLOAT;
    case SPIRType::Int:
        return UNIFORM_INT;
    case SPIRType::UInt:
        return UNIFORM_UINT;
    case SPIRType::Boolean:
        return UNIFORM_BOOL;
    default:
        return UNIFORM_UNKNOWN;
    }
}

void Shader::buildUniformBlockMembers(spirv_cross::Compiler &comp, const spirv_cross::SPIRType &type, size_t baseoff, const std::string &basename, const std::string &blockName)
{
    std::printf("[UBO DEBUG] buildUniformBlockMembers: %zu members in block %s\n", type.member_types.size(), blockName.c_str());
    
    for (size_t i = 0; i < type.member_types.size(); i++)
    {
        try {
            auto &membertype = comp.get_type(type.member_types[i]);
            std::string membername = comp.get_member_name(type.self, i);
            std::string fullname = basename + membername;
            
            std::printf("[UBO DEBUG] Processing member %zu: %s\n", i, fullname.c_str());
            
            size_t offset = baseoff + comp.type_struct_member_offset(type, i);
            size_t membersize = comp.get_declared_struct_member_size(type, i);
            
            std::printf("[UBO DEBUG] Member offset: %zu, size: %zu\n", offset, membersize);
            
            if (membertype.basetype == spirv_cross::SPIRType::Struct)
            {
                std::printf("[UBO DEBUG] Member is nested struct, recursing\n");
                // Nested struct - recurse
                buildUniformBlockMembers(comp, membertype, offset, fullname + ".", blockName);
            }
            else
            {
                std::printf("[UBO DEBUG] Creating UniformInfo for member\n");
                // Create UniformInfo for this member
                UniformInfo *info = new UniformInfo();
                info->name = fullname;
                info->location = -1; // UBO members don't have locations
                info->count = std::max(1u, (membertype.array.empty() ? 1u : membertype.array[0]));
                info->baseType = getUniformBaseType(membertype);
                info->components = membertype.columns > 1 ? membertype.columns : membertype.vecsize;
                if (membertype.columns > 1)
                {
                    info->matrix.columns = membertype.columns;
                    info->matrix.rows = membertype.vecsize;
                }
                info->data = nullptr;
                info->dataSize = membersize;
                info->active = false; // Will be set to true when the UBO is used
                info->stageMask = 0;
                info->dataBaseType = DATA_BASETYPE_FLOAT; // Default, could be improved
                info->textureType = TEXTURE_MAX_ENUM;
                info->access = ACCESS_READ;
                info->isDepthSampler = false;
                info->storageTextureFormat = PIXELFORMAT_UNKNOWN;
                info->bufferStride = 0;
                info->bufferMemberCount = 0;
                info->resourceIndex = -1;
                info->bindingStartIndex = -1;
                
                std::printf("[UBO DEBUG] Adding to reflection: %s\n", fullname.c_str());
                // Store in reflection
                reflection.allUniforms[fullname] = info;
                std::printf("[UBO DEBUG] Successfully added member\n");
            }
        } catch (const std::exception& e) {
            std::printf("[UBO ERROR] Exception processing member %zu: %s\n", i, e.what());
            throw;
        } catch (...) {
            std::printf("[UBO ERROR] Unknown exception processing member %zu\n", i);
            throw;
        }
    }
    std::printf("[UBO DEBUG] Finished processing all members for block %s\n", blockName.c_str());
}

void Shader::setUniformBuffer(const std::string &name, love::gfx::Buffer *buffer)
{
    auto it = uniformBufferBlocks.find(name);
    if (it == uniformBufferBlocks.end())
    {
        // UBO not found - could be a warning or error
        return;
    }
    
    UniformBufferInfo &uboInfo = it->second;
    uboInfo.buffer = buffer;
    
    // Set up the descriptor buffer info
    if (buffer != nullptr)
    {
        uboInfo.descriptorInfo.buffer = (VkBuffer)buffer->getHandle();
        uboInfo.descriptorInfo.offset = 0;
        uboInfo.descriptorInfo.range = uboInfo.size;
        
        // PERF: Don't mark dirty - causes 89 descriptor reallocations per frame
        // resourceDescriptorsDirty = true;
    }
}

// Replace the compileShaders function with this safer implementation:

void Shader::compileShaders()
{
    using namespace glslang;
    using namespace spirv_cross;

    std::printf("[UBO DEBUG] Starting shader compilation\n");

    std::vector<std::unique_ptr<TShader>> glslangShaders;
    auto program = std::make_unique<TProgram>();
    const auto &enabledExtensions = vgfx->getEnabledOptionalDeviceExtensions();

    // Instance variable instead of static to prevent memory corruption
    bool defaultBlockProcessedForThisShader = false;

    for (int i = 0; i < SHADERSTAGE_MAX_ENUM; i++)
    {
        if (!stages[i])
            continue;

        auto stage = (ShaderStageType)i;
        if (stage == SHADERSTAGE_COMPUTE)
            isCompute = true;

        auto glslangShaderStage = getGlslShaderType(stage);
        auto tshader = std::make_unique<TShader>(glslangShaderStage);

        tshader->setEnvInput(EShSourceGlsl, glslangShaderStage, EShClientVulkan, 450);
        tshader->setEnvClient(EShClientVulkan, EShTargetVulkan_1_2);
        if (enabledExtensions.spirv14)
            tshader->setEnvTarget(EshTargetSpv, EShTargetSpv_1_4);
        else
            tshader->setEnvTarget(EshTargetSpv, EShTargetSpv_1_0);
        tshader->setAutoMapLocations(true);
        tshader->setAutoMapBindings(true);
        tshader->setEnvInputVulkanRulesRelaxed();
        tshader->setGlobalUniformBinding(0);
        tshader->setGlobalUniformSet(0);

        auto &glsl = stages[i]->getSource();
        const char *csrc = glsl.c_str();
        const int sourceLength = static_cast<int>(glsl.length());
        tshader->setStringsWithLengths(&csrc, &sourceLength, 1);

        int defaultVersion = 450;
        EProfile defaultProfile = ECoreProfile;
        bool forceDefault = false;
        bool forwardCompat = true;

        if (!tshader->parse(GetResources(), defaultVersion, defaultProfile, forceDefault, forwardCompat, EShMsgSuppressWarnings))
        {
            const char *stageName = "unknown";
            ShaderStage::getConstant(stage, stageName);

            std::string err = "Error parsing " + std::string(stageName) + " shader:\n\n"
                + std::string(tshader->getInfoLog()) + "\n"
                + std::string(tshader->getInfoDebugLog());

            throw love::Exception("%s", err.c_str());
        }

        program->addShader(tshader.get());
        glslangShaders.push_back(std::move(tshader));
    }

    if (!program->link(EShMsgDefault))
        throw love::Exception("link failed! %s\n", program->getInfoLog());

    if (!program->mapIO())
        throw love::Exception("mapIO failed");

    BindingMapper bindingMapper(spv::DecorationBinding);
    BindingMapper ioLocationMapper(spv::DecorationLocation);
    BindingMapper vertexInputLocationMapper(spv::DecorationLocation);

    std::printf("[UBO DEBUG] Processing shader stages\n");

    for (int i = 0; i < SHADERSTAGE_MAX_ENUM; i++)
    {
        auto shaderStage = (ShaderStageType)i;
        auto glslangStage = getGlslShaderType(shaderStage);
        auto intermediate = program->getIntermediate(glslangStage);

        if (intermediate == nullptr)
            continue;

        spv::SpvBuildLogger logger;
        glslang::SpvOptions opt;
        opt.validate = true;

        std::vector<uint32> spirv;

        GlslangToSpv(*intermediate, spirv, &logger, &opt);

        auto compiler = std::make_unique<spirv_cross::CompilerGLSL>(spirv);
        auto &comp = *compiler;

        auto active = compiler->get_active_interface_variables();
        auto shaderResources = comp.get_shader_resources();

        std::printf("[UBO DEBUG] Processing uniform buffers for stage %d\n", i);

        // Process uniform buffers with safe error handling
        for (const auto &resource : shaderResources.uniform_buffers)
        {
            try {
                if (active.find(resource.id) == active.end()) {
                    std::printf("[UBO DEBUG] Skipping inactive uniform buffer: %s\n", resource.name.c_str());
                    continue;
                }

                std::printf("[UBO DEBUG] Processing uniform buffer: %s\n", resource.name.c_str());

                if (resource.name == "gl_DefaultUniformBlock")
                {
                    // Use instance variable instead of static
                    if (defaultBlockProcessedForThisShader) {
                        std::printf("[UBO DEBUG] Default uniform block already processed for this shader\n");
                        continue;
                    }
                    defaultBlockProcessedForThisShader = true;
                    
                    std::printf("[UBO DEBUG] Processing default uniform block\n");
                    
                    const auto &type = comp.get_type(resource.base_type_id);
                    size_t defaultUniformBlockSize = comp.get_declared_struct_size(type);

                    localUniformStagingData.resize(defaultUniformBlockSize);
                    localUniformData.resize(defaultUniformBlockSize);
                    localUniformLocation = bindingMapper(comp, spirv, resource.name, 1, resource.id);

                    memset(localUniformStagingData.data(), 0, defaultUniformBlockSize);
                    memset(localUniformData.data(), 0, defaultUniformBlockSize);

                    std::string basename("");
                    buildLocalUniforms(comp, type, 0, basename);
                    
                    std::printf("[UBO DEBUG] Default uniform block processed successfully\n");
                }
                else
                {
                    // Check if we've already processed this UBO
                    if (uniformBufferBlocks.find(resource.name) != uniformBufferBlocks.end()) {
                        std::printf("[UBO DEBUG] UBO %s already processed\n", resource.name.c_str());
                        continue;
                    }
                    
                    std::printf("[UBO DEBUG] Processing custom UBO: %s\n", resource.name.c_str());
                    
                    // Process custom uniform buffer blocks with error handling
                    std::string blockName = resource.name;
                    
                    auto blockType = comp.get_type(resource.base_type_id);
                    size_t blockSize = comp.get_declared_struct_size(blockType);
                    
                    UniformBufferInfo uboInfo;
                    uboInfo.name = blockName;
                    uboInfo.size = blockSize;
                    uboInfo.binding = comp.get_decoration(resource.id, spv::DecorationBinding);
                    
                    if (comp.has_decoration(resource.id, spv::DecorationDescriptorSet)) {
                        uboInfo.set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
                    } else {
                        uboInfo.set = 0;
                    }
                    
                    // Initialize descriptor info to safe defaults
                    uboInfo.descriptorInfo.buffer = VK_NULL_HANDLE;
                    uboInfo.descriptorInfo.offset = 0;
                    uboInfo.descriptorInfo.range = blockSize;
                    
                    uniformBufferBlocks[blockName] = uboInfo;
                    
                    std::printf("[UBO DEBUG] UBO info stored, processing members\n");
                    
                    // Process members with error handling
                    std::string basename = blockName + ".";
                    buildUniformBlockMembers(comp, blockType, 0, basename, blockName);
                    
                    std::printf("[UBO DEBUG] Custom UBO %s processed successfully\n", blockName.c_str());
                }
            } catch (const std::exception& e) {
                std::printf("[UBO ERROR] Exception processing uniform buffer %s: %s\n", resource.name.c_str(), e.what());
                // Continue processing other uniform buffers instead of crashing
                continue;
            } catch (...) {
                std::printf("[UBO ERROR] Unknown exception processing uniform buffer %s\n", resource.name.c_str());
                continue;
            }
        }

        std::printf("[UBO DEBUG] Finished processing uniform buffers for stage %d\n", i);

        // Continue with the rest of the shader processing...
        // (sampled_images, storage_buffers, etc. - same as original code)
        
        for (const auto &r : shaderResources.sampled_images)
        {
            if (active.find(r.id) == active.end())
                continue;

            std::string name = canonicaliizeUniformName(r.name);
            auto uniformit = reflection.allUniforms.find(name);
            if (uniformit == reflection.allUniforms.end())
            {
                handleUnknownUniformName(name.c_str());
                continue;
            }

            UniformInfo &u = *(uniformit->second);
            u.active = true;
            u.location = bindingMapper(comp, spirv, name, u.count, r.id);

            BuiltinUniform builtin;
            if (getConstant(name.c_str(), builtin))
                builtinUniformInfo[builtin] = &u;
        }

        for (const auto &r : shaderResources.storage_buffers)
        {
            if (active.find(r.id) == active.end())
                continue;

            std::string name = canonicaliizeUniformName(r.name);
            const auto &uniformit = reflection.storageBuffers.find(name);
            if (uniformit == reflection.storageBuffers.end())
            {
                handleUnknownUniformName(name.c_str());
                continue;
            }

            UniformInfo &u = uniformit->second;
            u.active = true;
            u.location = bindingMapper(comp, spirv, name, u.count, r.id);
        }

        for (const auto &r : shaderResources.storage_images)
        {
            if (active.find(r.id) == active.end())
                continue;

            std::string name = canonicaliizeUniformName(r.name);
            const auto &uniformit = reflection.storageTextures.find(name);
            if (uniformit == reflection.storageTextures.end())
            {
                handleUnknownUniformName(name.c_str());
                continue;
            }

            UniformInfo &u = uniformit->second;
            u.active = true;
            u.location = bindingMapper(comp, spirv, name, u.count, r.id);
        }

        // Check for push constants in all shader stages (not just vertex)
        auto pushConstants = comp.get_shader_resources().push_constant_buffers;
        for (const auto& pc : pushConstants) {
            const auto& type = comp.get_type(pc.base_type_id);
            size_t size = comp.get_declared_struct_size(type);
            uint32_t offset = 0; // Usually 0

            // Accumulate stage flags if the same block is used in multiple stages
            bool found = false;
            for (auto& range : pushConstantRanges) {
                if (range.offset == offset && range.size == size) {
                    range.stageFlags |= getStageBit(shaderStage);
                    found = true;
                    break;
                }
            }
            if (!found) {
                VkPushConstantRange range = {};
                range.stageFlags = getStageBit(shaderStage);
                range.offset = offset;
                range.size = (uint32_t)size;
                pushConstantRanges.push_back(range);
            }
        }

        if (shaderStage == SHADERSTAGE_VERTEX)
        {
            std::map<std::string, int> vertexInputs;
            
            for (const auto& attr : attributes)
            {
                vertexInputs[attr.first] = attr.second.index;
            }
            
            for (const auto &r : shaderResources.stage_inputs)
            {
                auto it = vertexInputs.find(r.name);
                if (it != vertexInputs.end() && it->second >= 0)
                    vertexInputLocationMapper(comp, spirv, r.name, 1, r.id);
            }

            for (const auto &r : shaderResources.stage_inputs)
            {
                int index = (int)vertexInputLocationMapper(comp, spirv, r.name, 1, r.id);

                DataBaseType basetype = DATA_BASETYPE_FLOAT;

                switch (comp.get_type(r.base_type_id).basetype)
                {
                case spirv_cross::SPIRType::Int:
                    basetype = DATA_BASETYPE_INT;
                    break;
                case spirv_cross::SPIRType::UInt:
                    basetype = DATA_BASETYPE_UINT;
                    break;
                default:
                    break;
                }

                attributes[r.name] = { index, basetype };
                vertexInputs[r.name] = index;
            }

            for (const auto &r : shaderResources.stage_outputs)
            {
                const auto &type = comp.get_type(r.base_type_id);
                int count = type.array.empty() ? 1 : type.array[0];
                if (type.op == spv::OpTypeMatrix)
                    count *= type.columns;

                ioLocationMapper(comp, spirv, r.name, count, r.id);
            }
        }
        else if (shaderStage == SHADERSTAGE_PIXEL)
        {
            for (const auto &r : shaderResources.stage_inputs)
            {
                const auto &type = comp.get_type(r.base_type_id);
                int count = type.array.empty() ? 1 : type.array[0];
                if (type.op == spv::OpTypeMatrix)
                    count *= type.columns;

                ioLocationMapper(comp, spirv, r.name, count, r.id);
            }
        }

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = spirv.size() * sizeof(uint32_t);
        createInfo.pCode = spirv.data();

        VkShaderModule shaderModule;

        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
            throw love::Exception("failed to create shader module");

        std::string debugname = getShaderStageDebugName(shaderStage);
        if (!debugname.empty() && vgfx->getEnabledOptionalInstanceExtensions().debugInfo)
        {
            auto device = vgfx->getDevice();

            VkDebugMarkerObjectNameInfoEXT nameInfo{};
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
            nameInfo.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT;
            nameInfo.object = (uint64_t)shaderModule;
            nameInfo.pObjectName = debugname.c_str();
            vkDebugMarkerSetObjectNameEXT(device, &nameInfo);
        }

        shaderModules.push_back(shaderModule);

        VkPipelineShaderStageCreateInfo shaderStageInfo{};
        shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageInfo.stage = getStageBit((ShaderStageType)i);
        shaderStageInfo.module = shaderModule;
        shaderStageInfo.pName = "main";

        shaderStages.push_back(shaderStageInfo);
    }

    std::printf("[UBO DEBUG] Shader compilation completed successfully\n");

#ifdef _WIN32
	BOOL heapOk = HeapValidate(GetProcessHeap(), 0, nullptr);
	if (!heapOk) {
		printf("[SHADER HEAP] HeapValidate FAILED after shader compilation!\n");
		fflush(stdout);
	}
#endif

    
    int numBuffers = 0;
    int numTextures = 0;
    int numBufferViews = 0;

    if (localUniformData.size() > 0)
        numBuffers++;

    for (const auto &kvp : reflection.allUniforms)
    {
        if (!kvp.second->active)
            continue;

        switch (kvp.second->baseType)
        {
        case UNIFORM_SAMPLER:
        case UNIFORM_STORAGETEXTURE:
            numTextures += kvp.second->count;
            break;
        case UNIFORM_STORAGEBUFFER:
            numBuffers += kvp.second->count;
            break;
        case UNIFORM_TEXELBUFFER:
            numBufferViews += kvp.second->count;
            break;
        default:
            continue;
        }
    }

    // Clear vectors at START of descriptor building, then pre-reserve
    // to ensure no reallocation happens during push_back operations
    descriptorWrites.clear();
    descriptorBuffers.clear();
    descriptorImages.clear();
    descriptorBufferViews.clear();
    allTextureInfo.clear();
    storageBufferInfo.clear();
    
    // Now pre-reserve capacity to prevent reallocation
	descriptorBuffers.reserve(numBuffers + 16);  // +16 safety margin

	descriptorImages.reserve(numTextures + 16);  // +16 safety margin

	// Pre-reserve capacity after clearing
	descriptorBufferViews.reserve(numBufferViews + 16);  // +16 safety margin
	allTextureInfo.reserve(numTextures + 16);  // +16 safety margin
	storageBufferInfo.reserve(numBuffers + 16);  // +16 safety margin

	// Pre-reserve descriptorWrites to prevent reallocation invalidating pointers in VkWriteDescriptorSet
	size_t maxDescriptorWrites = numBuffers + numTextures + numBufferViews + 16;
	descriptorWrites.reserve(maxDescriptorWrites);

	if (localUniformData.size() > 0)
	{
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.range = localUniformData.size();

		descriptorBuffers.push_back(bufferInfo);
		storageBufferInfo.push_back({ nullptr, ACCESS_READ });

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstBinding = localUniformLocation;
		write.dstArrayElement = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		write.descriptorCount = 1;
		write.pBufferInfo = &descriptorBuffers.back();
		descriptorWrites.push_back(write);
	}

	for (auto &u : reflection.sampledTextures)
    {
        UniformInfo &info = u.second;
        if (!info.active)
            continue;

        info.bindingStartIndex = (int)descriptorImages.size();

		for (int i = 0; i < info.count; i++)
		{
			VkDescriptorImageInfo imageInfo{};
			descriptorImages.push_back(imageInfo);

			allTextureInfo.push_back({ nullptr, info.access });

			int idx = info.resourceIndex + i;
			if (idx >= 0 && idx < (int)activeTextures.size())
			{
				auto texture = activeTextures[idx];
				if (texture != nullptr)
					setTextureDescriptor(&info, texture, i);
			}
		}

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstBinding = info.location;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = static_cast<uint32_t>(info.count);
        write.pImageInfo = &descriptorImages[info.bindingStartIndex];

        descriptorWrites.push_back(write);
    }

    for (auto &u : reflection.storageTextures)
    {
        UniformInfo &info = u.second;
        if (!info.active)
            continue;

        info.bindingStartIndex = (int)descriptorImages.size();

		for (int i = 0; i < info.count; i++)
		{
			VkDescriptorImageInfo imageInfo{};
			descriptorImages.push_back(imageInfo);

			allTextureInfo.push_back({ nullptr, info.access });

			int idx = info.resourceIndex + i;
			if (idx >= 0 && idx < (int)activeTextures.size())
			{
				auto texture = activeTextures[idx];
				if (texture != nullptr)
					setTextureDescriptor(&info, texture, i);
			}
		}

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstBinding = info.location;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write.descriptorCount = static_cast<uint32_t>(info.count);
        write.pImageInfo = &descriptorImages[info.bindingStartIndex];

        descriptorWrites.push_back(write);
    }

    for (auto &u : reflection.texelBuffers)
    {
        UniformInfo &info = u.second;
        if (!info.active)
            continue;

        info.bindingStartIndex = (int)descriptorBufferViews.size();

		for (int i = 0; i < info.count; i++)
		{
			descriptorBufferViews.push_back(VK_NULL_HANDLE);

			int idx = info.resourceIndex + i;
			if (idx >= 0 && idx < (int)activeBuffers.size())
			{
				auto buffer = activeBuffers[idx];
				if (buffer != nullptr)
					setBufferDescriptor(&info, buffer, i);
			}
		}

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstBinding = info.location;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        write.descriptorCount = info.count;
        write.pTexelBufferView = &descriptorBufferViews[info.bindingStartIndex];

        descriptorWrites.push_back(write);
    }

    for (auto &u : reflection.storageBuffers)
    {
        UniformInfo &info = u.second;
        if (!info.active)
            continue;

        info.bindingStartIndex = (int)descriptorBuffers.size();

		for (int i = 0; i < info.count; i++)
		{
			VkDescriptorBufferInfo bufferInfo{};
			descriptorBuffers.push_back(bufferInfo);

			storageBufferInfo.push_back({ nullptr, info.access });

			int idx = info.resourceIndex + i;
			if (idx >= 0 && idx < (int)activeBuffers.size())
			{
				auto buffer = activeBuffers[idx];
				if (buffer != nullptr)
					setBufferDescriptor(&info, buffer, i);
			}
		}

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstBinding = info.location;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = info.count;
        write.pBufferInfo = &descriptorBuffers[info.bindingStartIndex];

        descriptorWrites.push_back(write);
    }

    // PERF: Don't mark dirty - causes 89 descriptor reallocations per frame
    // resourceDescriptorsDirty = true;
}

void Shader::updateBufferInternal(std::string name, const void* data, size_t size, size_t offset)
{
    auto ssboIt = reflection.storageBuffers.find(name);
    if (ssboIt != reflection.storageBuffers.end())
    {
        UniformInfo &info = ssboIt->second;
        if (info.resourceIndex < 0 || info.resourceIndex >= (int)activeBuffers.size())
            throw love::Exception("Invalid resource index for storage buffer %s.", name.c_str());
        love::gfx::Buffer* buffer = activeBuffers[info.resourceIndex];
        if (!buffer)
            throw love::Exception("No buffer bound for storage buffer %s.", name.c_str());
        size_t bufferSize = buffer->getSize();
        // std::printf("[DEBUG] Updating storage buffer %s (data size: %zu, buffer size: %zu)\n", name.c_str(), size, bufferSize);
        if (size > bufferSize)
            throw love::Exception("Data size exceeds storage buffer %s size.", name.c_str());
        
        // std::printf("[DEBUG] Buffer handle: %p, size: %zu\n", (void*)buffer->getHandle(), buffer->getSize());
        // std::printf("[DEBUG] Filling buffer at offset %zu, size %zu\n", 0, size);
        // const uint8_t* bytes = static_cast<const uint8_t*>(data);
        // for (size_t i = 0; i < std::min(size_t(32), size); ++i)
        //     std::printf("%02x ", bytes[i]);
        // std::printf("\n");
        buffer->fill(offset, size, data);

        // Force descriptor update
        // resourceDescriptorsDirty = true;
        // setBufferDescriptor(&info, buffer, 0);
        return;
    }
}

void Shader::createDescriptorSetLayout()
{
	std::vector<VkDescriptorSetLayoutBinding> bindings;

	for (auto const &entry : reflection.allUniforms)
	{
		if (!entry.second->active)
			continue;

		auto type = Vulkan::getDescriptorType(entry.second->baseType);
		if (type != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
		{
			VkDescriptorSetLayoutBinding layoutBinding{};

			layoutBinding.binding = entry.second->location;
			layoutBinding.descriptorType = type;
			layoutBinding.descriptorCount = entry.second->count;
			layoutBinding.stageFlags = getStageFlags((ShaderStageMask)entry.second->stageMask);

			bindings.push_back(layoutBinding);
		}
	}

	if (!localUniformStagingData.empty())
	{
		VkDescriptorSetLayoutBinding uniformBinding{};
		uniformBinding.binding = localUniformLocation;
		uniformBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		uniformBinding.descriptorCount = 1;
		if (isCompute)
			uniformBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		else
			uniformBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings.push_back(uniformBinding);
	}

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
		throw love::Exception("failed to create descriptor set layout");
}

void Shader::createPipelineLayout()
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
	pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
		throw love::Exception("failed to create pipeline layout");

	if (isCompute)
	{
		// assert(shaderStages.size() == 1);

		VkComputePipelineCreateInfo computeInfo{};
		computeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computeInfo.stage = shaderStages.at(1);
		computeInfo.layout = pipelineLayout;

		if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computeInfo, nullptr, &computePipeline) != VK_SUCCESS)
			throw love::Exception("failed to create compute pipeline");
	}
}

void Shader::createDescriptorPoolSizes()
{
	if (!localUniformData.empty())
	{
		VkDescriptorPoolSize size{};
		size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		size.descriptorCount = 1;

		descriptorPoolSizes.push_back(size);
	}

	for (const auto &entry : reflection.allUniforms)
	{
		if (!entry.second->active)
			continue;

		VkDescriptorPoolSize size{};
		auto type = Vulkan::getDescriptorType(entry.second->baseType);
		if (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
			continue;

		size.type = type;
		size.descriptorCount = entry.second->count;
		descriptorPoolSizes.push_back(size);
	}
}

void Shader::setTextureDescriptor(const UniformInfo *info, love::gfx::Texture *texture, int index)
{
	static int callCounter = 0;
	callCounter++;
	
	// Safety checks
	if (!info)
		throw love::Exception("setTextureDescriptor: info is null");
	if (index < 0 || index >= info->count)
		throw love::Exception("setTextureDescriptor: index %d out of bounds for count %d", index, info->count);
	
	if (info->bindingStartIndex < 0)
		throw love::Exception("setTextureDescriptor: invalid bindingStartIndex %d", info->bindingStartIndex);
	
	size_t descriptorIndex = info->bindingStartIndex + index;

	if (descriptorIndex >= descriptorImages.size())
		throw love::Exception("setTextureDescriptor: descriptor index %zu out of bounds (size: %zu)", descriptorIndex, descriptorImages.size());
	
	if (descriptorIndex >= allTextureInfo.size())
		throw love::Exception("setTextureDescriptor: texture info index %zu out of bounds (size: %zu)", descriptorIndex, allTextureInfo.size());
	
	// Read current state by value, then write back by index at the end.
	VkImageView currentView = descriptorImages[descriptorIndex].imageView;
	VkImageLayout currentLayout = descriptorImages[descriptorIndex].imageLayout;

    // Only proceed when the incoming texture is a Vulkan texture instance. In rare
    // cases a stale or backend-mismatched pointer could arrive here (e.g. a
    // destroyed mesh still being referenced). If the dynamic cast fails, skip
    // binding to avoid crashing and leave the descriptor unbound.
    Texture* vkTexture = nullptr;
    if (texture)
    {
        vkTexture = dynamic_cast<Texture*>(texture);
        if (!vkTexture)
        {
            // Write directly by index, not via reference
            descriptorImages[descriptorIndex].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            descriptorImages[descriptorIndex].imageView = VK_NULL_HANDLE;
            allTextureInfo[descriptorIndex].texture = nullptr;
            resourceDescriptorsDirty = true;
            return;
        }
    }

	// Samplers may change after this call, so they're set just before the
	// descriptor set is used instead of here.
	VkImageView view = vkTexture != nullptr ? (VkImageView)vkTexture->getHandle() : VK_NULL_HANDLE;
	if (view != currentView)
	{
		// Transition render targets from COLOR_ATTACHMENT_OPTIMAL to SHADER_READ_ONLY_OPTIMAL if needed
		if (vkTexture != nullptr)
		{
			vkTexture->transitionForSampling();
		}
		// Now use the actual layout from the texture (which should be SHADER_READ_ONLY_OPTIMAL after transition)
		VkImageLayout newLayout = vkTexture != nullptr ? vkTexture->getImageLayout() : VK_IMAGE_LAYOUT_UNDEFINED;
		// Write directly by index after all potentially-reallocating calls
		descriptorImages[descriptorIndex].imageLayout = newLayout;
		descriptorImages[descriptorIndex].imageView = view;
		allTextureInfo[descriptorIndex].texture = texture;
		resourceDescriptorsDirty = true;
	}
	else
	{
	}
}

void Shader::setMainTex(gfx::Texture *texture)
{
	const UniformInfo *info = getUniformInfo(BUILTIN_TEXTURE_MAIN);
	if (info != nullptr)
		setTextureDescriptor(info, texture, 0);
}

void Shader::setBufferDescriptor(const UniformInfo *info, love::gfx::Buffer *buffer, int index)
{
	// Safety checks
	if (!info)
		throw love::Exception("setBufferDescriptor: info is null");
	if (index < 0 || index >= info->count)
		throw love::Exception("setBufferDescriptor: index %d out of bounds for count %d", index, info->count);
	
	if (info->bindingStartIndex < 0)
		throw love::Exception("setBufferDescriptor: invalid bindingStartIndex %d", info->bindingStartIndex);
	
	size_t bufferIndex = info->bindingStartIndex + index;
	
	if (info->baseType == UNIFORM_STORAGEBUFFER)
	{
		if (bufferIndex >= descriptorBuffers.size())
			throw love::Exception("setBufferDescriptor: storage buffer index %zu out of bounds (size: %zu)", bufferIndex, descriptorBuffers.size());
		
		if (bufferIndex >= storageBufferInfo.size())
			throw love::Exception("setBufferDescriptor: storage buffer info index %zu out of bounds (size: %zu)", bufferIndex, storageBufferInfo.size());
		
		VkDescriptorBufferInfo &bufferInfo = descriptorBuffers[bufferIndex];
		VkBuffer vkbuffer = buffer != nullptr ? (VkBuffer)buffer->getHandle() : VK_NULL_HANDLE;
		VkDeviceSize range = buffer != nullptr ? buffer->getSize() : 0;
		if (vkbuffer != bufferInfo.buffer || bufferInfo.offset != 0 || range != bufferInfo.range)
		{
			bufferInfo.buffer = vkbuffer;
			bufferInfo.offset = 0;
			bufferInfo.range = range;
			storageBufferInfo[bufferIndex].buffer = buffer;
			resourceDescriptorsDirty = true;
		}
	}
	else if (info->baseType == UNIFORM_TEXELBUFFER)
	{
		if (bufferIndex >= descriptorBufferViews.size())
			throw love::Exception("setBufferDescriptor: texel buffer index %zu out of bounds (size: %zu)", bufferIndex, descriptorBufferViews.size());
		
		VkBufferView view = buffer != nullptr ? (VkBufferView)buffer->getTexelBufferHandle() : VK_NULL_HANDLE;
		if (view != descriptorBufferViews[bufferIndex])
		{
			descriptorBufferViews[bufferIndex] = view;
			resourceDescriptorsDirty = true;
		}
	}
}

void Shader::createDescriptorPool()
{
	VkDescriptorPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	// WORKAROUND #3: Enable explicit freeing for AMD GPU driver
	createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	createInfo.maxSets = DESCRIPTOR_POOL_SIZE;
	createInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	createInfo.pPoolSizes = descriptorPoolSizes.data();

	VkDescriptorPool pool;
	if (vkCreateDescriptorPool(device, &createInfo, nullptr, &pool) != VK_SUCCESS)
		throw love::Exception("failed to create descriptor pool");

	descriptorPools[currentFrame].push_back(pool);
}

VkDescriptorSet Shader::allocateDescriptorSet()
{
	// Safety checks to prevent segfaults
	if (device == VK_NULL_HANDLE)
		throw love::Exception("Cannot allocate descriptor set: device is null");
	
	if (descriptorSetLayout == VK_NULL_HANDLE)
		throw love::Exception("Cannot allocate descriptor set: descriptorSetLayout is null");
	
	if (currentFrame >= MAX_FRAMES_IN_FLIGHT)
		throw love::Exception("Cannot allocate descriptor set: currentFrame (%zu) is out of range", currentFrame);

	// Limit the number of pools to prevent unbounded growth
	const size_t MAX_DESCRIPTOR_POOLS = 10;
	
	if (descriptorPools[currentFrame].empty())
		createDescriptorPool();

	while (true)
	{
		VkDescriptorPool pool = descriptorPools[currentFrame][currentDescriptorPool];

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &descriptorSetLayout;

		VkDescriptorSet descriptorSet;
		VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);

		switch (result)
		{
		case VK_SUCCESS:
			// WORKAROUND #3: Track allocated descriptor sets for explicit freeing (AMD GPU)
			allocatedDescriptorSets[currentFrame].push_back({pool, descriptorSet});
			return descriptorSet;
		case VK_ERROR_OUT_OF_POOL_MEMORY_KHR:
		case VK_ERROR_FRAGMENTED_POOL:
			currentDescriptorPool++;
			if (currentDescriptorPool >= descriptorPools[currentFrame].size())
				createDescriptorPool();
			continue;
		default:
			throw love::Exception("failed to allocate descriptor set (VkResult = %d)", result);
		}
	}
}

std::array<VkPipeline, 2> Shader::getCachedGraphicsPipeline(Graphics *vgfx, const GraphicsPipelineConfigurationCore &configuration)
{
	auto it = graphicsPipelinesDynamicState.find(configuration);
	if (it != graphicsPipelinesDynamicState.end())
		return it->second;

	std::array<VkPipeline, 2> pipeline = vgfx->createGraphicsPipeline(this, configuration, nullptr);
	graphicsPipelinesDynamicState.insert({ configuration, pipeline });
	
	return pipeline;
}

std::array<VkPipeline, 2> Shader::getCachedGraphicsPipeline(Graphics *vgfx, const GraphicsPipelineConfigurationFull &configuration)
{
	auto it = graphicsPipelinesNoDynamicState.find(configuration);
	if (it != graphicsPipelinesNoDynamicState.end())
		return it->second;

	std::array<VkPipeline, 2> pipeline = vgfx->createGraphicsPipeline(this, configuration.core, &configuration.noDynamicState);
	graphicsPipelinesNoDynamicState.insert({ configuration, pipeline });
	
	return pipeline;
}

} // vulkan
} // graphics
} // love

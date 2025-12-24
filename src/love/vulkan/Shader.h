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

#pragma once

// LÖVE
#include "../common/Optional.h"
#include "../shader.h"
#include "ShaderStage.h"
#include "Vulkan.h"

// Libraries
// #include "VulkanWrapper.h"
#include "spirv_cross/spirv_reflect.hpp"
#include "../libraries/xxHash/xxhash.h"

// C++
#include <map>
#include <memory>
#include <unordered_map>
#include <queue>
#include <set>


namespace love
{
namespace gfx
{
namespace vulkan
{

struct GraphicsPipelineConfigurationCore
{
	VkRenderPass renderPass;
	VertexAttributes attributes;
	VertexAttributesID attributesID;
	bool wireFrame;
	uint32 blendStateKey;
	ColorChannelMask colorChannelMask;
	VkSampleCountFlagBits msaaSamples;
	uint32_t numColorAttachments;
	PrimitiveType primitiveType;
	uint64 packedColorAttachmentFormats;
	VkBool32 depthWriteEnable;
	VkCompareOp depthCompareOp;

	GraphicsPipelineConfigurationCore()
	{
		memset(this, 0, sizeof(GraphicsPipelineConfigurationCore));
	}

	bool operator==(const GraphicsPipelineConfigurationCore &other) const
	{
		return memcmp(this, &other, sizeof(GraphicsPipelineConfigurationCore)) == 0;
	}
};

struct GraphicsPipelineConfigurationCoreHasher
{
	size_t operator() (const GraphicsPipelineConfigurationCore &configuration) const
	{
		return XXH32(&configuration, sizeof(GraphicsPipelineConfigurationCore), 0);
	}
};

struct GraphicsPipelineConfigurationNoDynamicState
{
	CullMode cullmode = CULL_BACK;
	Winding winding = WINDING_MAX_ENUM;
	StencilAction stencilAction = STENCIL_MAX_ENUM;
	CompareMode stencilCompare = COMPARE_MAX_ENUM;
	DepthState depthState{};
};

struct GraphicsPipelineConfigurationFull
{
	GraphicsPipelineConfigurationCore core;
	GraphicsPipelineConfigurationNoDynamicState noDynamicState;

	GraphicsPipelineConfigurationFull()
	{
		memset(this, 0, sizeof(GraphicsPipelineConfigurationFull));
	}

	bool operator==(const GraphicsPipelineConfigurationFull &other) const
	{
		return memcmp(this, &other, sizeof(GraphicsPipelineConfigurationFull)) == 0;
	}
};

struct GraphicsPipelineConfigurationFullHasher
{
	size_t operator() (const GraphicsPipelineConfigurationFull &configuration) const
	{
		return XXH32(&configuration, sizeof(GraphicsPipelineConfigurationFull), 0);
	}
};

class Graphics;

class Shader final
	: public love::gfx::Shader
	, public Volatile
{
public:

	struct AttributeInfo
	{
		int index;
		DataBaseType baseType;
	};

	struct TextureInfo
	{
		love::gfx::Texture *texture;
		Access access;
	};

	struct BufferInfo
	{
		love::gfx::Buffer *buffer;
		Access access;
	};

	struct UniformBufferInfo
    {
        std::string name;
        size_t size;
        uint32_t binding;
        uint32_t set;
        love::gfx::Buffer *buffer = nullptr;
        VkDescriptorBufferInfo descriptorInfo;
    };

    // Add this method declaration in the private section (around line 185):
    void buildUniformBlockMembers(spirv_cross::Compiler &comp, const spirv_cross::SPIRType &type, size_t baseoff, const std::string &basename, const std::string &blockName);

    // Add this member variable in the private section (around line 235):
    std::unordered_map<std::string, UniformBufferInfo> uniformBufferBlocks;

	void setUniformBuffer(const std::string &name, love::gfx::Buffer *buffer);

	Shader(StrongRef<love::gfx::ShaderStage> stages[], const CompileOptions &options);
	virtual ~Shader();

	void setPushConstant(const UniformInfo *info, const void *data, int count);

	void updateBufferInternal(std::string name, const void *data, size_t size, size_t offset) override;
	
	bool loadVolatile() override;
	void unloadVolatile() override;

	VkPipeline getComputePipeline() const;

	const std::vector<VkPipelineShaderStageCreateInfo> &getShaderStages() const;

	const VkPipelineLayout getGraphicsPipelineLayout() const;

	void newFrame();

	void cmdPushDescriptorSets(VkCommandBuffer, VkPipelineBindPoint);

	void attach() override;

	ptrdiff_t getHandle() const override { return 0; }

	std::string getWarnings() const override { return ""; }

	int getVertexAttributeIndex(const std::string &name) override;
	const std::unordered_map<std::string, AttributeInfo> getVertexAttributeIndices() const { return attributes; }

	const UniformInfo *getUniformInfo(BuiltinUniform builtin) const override;

	void updateUniform(const UniformInfo *info, int count) override;

	void setMainTex(gfx::Texture *texture);

	VkPipeline getCachedGraphicsPipeline(Graphics *vgfx, const GraphicsPipelineConfigurationCore &configuration);
	VkPipeline getCachedGraphicsPipeline(Graphics *vgfx, const GraphicsPipelineConfigurationFull &configuration);

	const std::vector<TextureInfo> &getActiveTextureInfo() const { return allTextureInfo; }
	const std::vector<BufferInfo> &getActiveStorageBufferInfo() const { return storageBufferInfo; }

private:
	void compileShaders();
	void createDescriptorSetLayout();
	void createPipelineLayout();
	void createDescriptorPoolSizes();
    gfx::Shader::UniformType getUniformBaseType(const spirv_cross::SPIRType &type);
	void buildLocalUniforms(spirv_cross::Compiler &comp, const spirv_cross::SPIRType &type, size_t baseoff, const std::string &basename);
	void createDescriptorPool();
	VkDescriptorSet allocateDescriptorSet();

	void setTextureDescriptor(const UniformInfo *info, love::gfx::Texture *texture, int index);
	void setBufferDescriptor(const UniformInfo *info, love::gfx::Buffer *buffer, int index);

	void applyTexture(const UniformInfo *info, int i, love::gfx::Texture *texture, UniformType basetype, bool isdefault) override;
	void applyBuffer(const UniformInfo *info, int i, love::gfx::Buffer *buffer, UniformType basetype, bool isdefault) override;

	VkPipeline computePipeline = VK_NULL_HANDLE;

	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorPoolSize> descriptorPoolSizes;

	std::vector<std::vector<VkDescriptorPool>> descriptorPools;
	// Track allocated descriptor sets per frame for explicit freeing (AMD GPU workaround)
	std::vector<std::vector<std::pair<VkDescriptorPool, VkDescriptorSet>>> allocatedDescriptorSets;

	std::vector<VkDescriptorBufferInfo> descriptorBuffers;
	std::vector<VkDescriptorImageInfo> descriptorImages;
	std::vector<VkBufferView> descriptorBufferViews;
	std::vector<VkWriteDescriptorSet> descriptorWrites;

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	std::vector<VkShaderModule> shaderModules;

	std::vector<TextureInfo> allTextureInfo;
	std::vector<BufferInfo> storageBufferInfo;

	std::vector<VkPushConstantRange> pushConstantRanges;

	Graphics *vgfx = nullptr;
	VkDevice device = VK_NULL_HANDLE;

	bool isCompute = false;
	bool resourceDescriptorsDirty = false;
	VkDescriptorSet currentDescriptorSet = VK_NULL_HANDLE;

	UniformInfo *builtinUniformInfo[BUILTIN_MAX_ENUM];

	std::unique_ptr<StreamBuffer> uniformBufferObjectBuffer;
	std::vector<uint8> localUniformData;
	std::vector<uint8> localUniformStagingData;
	uint32_t localUniformLocation = 0;
	OptionalInt builtinUniformDataOffset;

	std::unordered_map<std::string, AttributeInfo> attributes;

	std::unordered_map<GraphicsPipelineConfigurationCore, VkPipeline, GraphicsPipelineConfigurationCoreHasher> graphicsPipelinesDynamicState;
	std::unordered_map<GraphicsPipelineConfigurationFull, VkPipeline, GraphicsPipelineConfigurationFullHasher> graphicsPipelinesNoDynamicState;

	uint32_t currentFrame = 0;
	uint32_t currentDescriptorPool = 0;
};

}
}
}

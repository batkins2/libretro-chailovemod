#include <string>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <optional>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <malloc.h>  // For _heapmin()
#endif
// #include "libretro.h"
#include "libretro_core_options.h"
#define __STDC_FORMAT_MACROS
#include "ChaiLove.h"
#include "LibretroLog.h"
#include <retro_dirent.h>
#include <streams/file_stream.h>
#include "libretro_vulkan.h"
// #include "../vendor/MemPlumber/memplumber.h"

// #if defined(HAVE_PSGL)
// #define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER_OES
// #define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_OES
// #define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
// #elif defined(OSX_PPC)
// #define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER_EXT
// #define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_EXT
// #define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
// #else
// #define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER
// #define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE
// #define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0
// #endif


static void fallback_log(enum retro_log_level level,
			 const char *fmt, ...);

static retro_video_refresh_t video_cb;
static struct retro_hw_render_callback hw_render;
static const struct retro_hw_render_interface_vulkan *vulkan;
extern retro_log_printf_t log_cb;

static struct {
    uint32_t index;
    VkImage images[8];  // Adjust size as needed
    VkCommandBuffer cmd[8];
} vk;

#ifdef JPH_DEBUG_RENDERER

// Simple vertex shader for debug rendering (SPIR-V bytecode)
// Input: layout(location = 0) in vec3 position; layout(location = 1) in vec4 color;
// Output: layout(location = 0) out vec4 fragColor;
// Vertex shader transforms position and passes color to fragment shader
// Simple vertex shader for debug rendering (SPIR-V bytecode)
// Simpler version: just passes through position and color
static const uint32_t debugVertexShaderSPIRV[] = {
	0x07230203, 0x00010000, 0x0008000a, 0x0000002d, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
	0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
	0x0009000f, 0x00000000, 0x00000004, 0x6e69616d, 0x00000000, 0x00000009, 0x0000000d, 0x00000015,
	0x00000028, 0x00030003, 0x00000002, 0x000001c2, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000,
	0x00060005, 0x00000009, 0x505f6c67, 0x65567265, 0x78657472, 0x00000000, 0x00060006, 0x00000009,
	0x00000000, 0x505f6c67, 0x7469736f, 0x006e6f69, 0x00030005, 0x0000000b, 0x00000000, 0x00040005,
	0x0000000d, 0x736f7061, 0x00000000, 0x00050005, 0x00000015, 0x67617266, 0x6f6c6f43, 0x00000072,
	0x00050005, 0x00000028, 0x6f6c6f63, 0x6e495f72, 0x00000000, 0x00050048, 0x00000009, 0x00000000,
	0x0000000b, 0x00000000, 0x00030047, 0x00000009, 0x00000002, 0x00040047, 0x0000000d, 0x0000001e,
	0x00000000, 0x00040047, 0x00000015, 0x0000001e, 0x00000000, 0x00040047, 0x00000028, 0x0000001e,
	0x00000001, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006,
	0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x0004001e, 0x00000009, 0x00000007,
	0x00040020, 0x0000000a, 0x00000003, 0x00000009, 0x0004003b, 0x0000000a, 0x0000000b, 0x00000003,
	0x00040015, 0x0000000c, 0x00000020, 0x00000001, 0x0004002b, 0x0000000c, 0x0000000e, 0x00000000,
	0x00040017, 0x0000000f, 0x00000006, 0x00000003, 0x00040020, 0x00000010, 0x00000001, 0x0000000f,
	0x0004003b, 0x00000010, 0x0000000d, 0x00000001, 0x0004002b, 0x00000006, 0x00000012, 0x3f800000,
	0x00040020, 0x00000014, 0x00000003, 0x00000007, 0x0004003b, 0x00000014, 0x00000015, 0x00000003,
	0x00040020, 0x00000027, 0x00000001, 0x00000007, 0x0004003b, 0x00000027, 0x00000028, 0x00000001,
	0x00040020, 0x0000002b, 0x00000003, 0x00000007, 0x00050036, 0x00000002, 0x00000004, 0x00000000,
	0x00000003, 0x000200f8, 0x00000005, 0x0004003d, 0x0000000f, 0x00000011, 0x0000000d, 0x00050051,
	0x00000006, 0x00000016, 0x00000011, 0x00000000, 0x00050051, 0x00000006, 0x00000017, 0x00000011,
	0x00000001, 0x00050051, 0x00000006, 0x00000018, 0x00000011, 0x00000002, 0x00070050, 0x00000007,
	0x00000019, 0x00000016, 0x00000017, 0x00000018, 0x00000012, 0x00050041, 0x0000002b, 0x0000001a,
	0x0000000b, 0x0000000e, 0x0003003e, 0x0000001a, 0x00000019, 0x0004003d, 0x00000007, 0x00000029,
	0x00000028, 0x0003003e, 0x00000015, 0x00000029, 0x000100fd, 0x00010038
};

// Simple fragment shader for debug rendering (SPIR-V bytecode)
static const uint32_t debugFragmentShaderSPIRV[] = {
	0x07230203, 0x00010000, 0x0008000a, 0x0000000d, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
	0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
	0x0007000f, 0x00000004, 0x00000004, 0x6e69616d, 0x00000000, 0x00000009, 0x0000000b, 0x00030010,
	0x00000004, 0x00000007, 0x00030003, 0x00000002, 0x000001c2, 0x00040005, 0x00000004, 0x6e69616d,
	0x00000000, 0x00050005, 0x00000009, 0x4374756f, 0x726f6c6f, 0x00000000, 0x00050005, 0x0000000b,
	0x67617266, 0x6f6c6f43, 0x00000072, 0x00040047, 0x00000009, 0x0000001e, 0x00000000, 0x00040047,
	0x0000000b, 0x0000001e, 0x00000000, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002,
	0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040020,
	0x00000008, 0x00000003, 0x00000007, 0x0004003b, 0x00000008, 0x00000009, 0x00000003, 0x00040020,
	0x0000000a, 0x00000001, 0x00000007, 0x0004003b, 0x0000000a, 0x0000000b, 0x00000001, 0x00050036,
	0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x0004003d, 0x00000007,
	0x0000000c, 0x0000000b, 0x0003003e, 0x00000009, 0x0000000c, 0x000100fd, 0x00010038
};

// Debug renderer state
struct DebugRendererState {
	VkPipeline linePipeline = VK_NULL_HANDLE;
	VkPipeline trianglePipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkShaderModule vertexShader = VK_NULL_HANDLE;
	VkShaderModule fragmentShader = VK_NULL_HANDLE;
	VkRenderPass debugRenderPass = VK_NULL_HANDLE;
	VkBuffer lineVertexBuffer = VK_NULL_HANDLE;
	VmaAllocation lineVertexAllocation = VK_NULL_HANDLE;
	size_t lineVertexBufferSize = 0;
	VkBuffer triVertexBuffer = VK_NULL_HANDLE;
	VmaAllocation triVertexAllocation = VK_NULL_HANDLE;
	size_t triVertexBufferSize = 0;
	bool initialized = false;
	bool failed = false;  // Set to true if initialization fails to prevent repeated attempts
	bool pipelinesCreated = false;  // Set to true once pipelines are created
};

static DebugRendererState g_debugRenderer;

// Disable debug rendering by default to avoid massive per-frame overhead.
// Re-enable only when actively debugging physics.
#define ENABLE_DEBUG_GEOMETRY_RENDERING 1

static void cleanupDebugRenderer(VmaAllocator allocator, VkDevice device) {
	if (allocator == VK_NULL_HANDLE || device == VK_NULL_HANDLE) {
		return;
	}
	
	if (g_debugRenderer.lineVertexBuffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(allocator, g_debugRenderer.lineVertexBuffer, g_debugRenderer.lineVertexAllocation);
		g_debugRenderer.lineVertexBuffer = VK_NULL_HANDLE;
	}
	if (g_debugRenderer.triVertexBuffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(allocator, g_debugRenderer.triVertexBuffer, g_debugRenderer.triVertexAllocation);
		g_debugRenderer.triVertexBuffer = VK_NULL_HANDLE;
	}
	if (g_debugRenderer.linePipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(device, g_debugRenderer.linePipeline, nullptr);
		g_debugRenderer.linePipeline = VK_NULL_HANDLE;
	}
	if (g_debugRenderer.trianglePipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(device, g_debugRenderer.trianglePipeline, nullptr);
		g_debugRenderer.trianglePipeline = VK_NULL_HANDLE;
	}
	if (g_debugRenderer.debugRenderPass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(device, g_debugRenderer.debugRenderPass, nullptr);
		g_debugRenderer.debugRenderPass = VK_NULL_HANDLE;
	}
	if (g_debugRenderer.pipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(device, g_debugRenderer.pipelineLayout, nullptr);
		g_debugRenderer.pipelineLayout = VK_NULL_HANDLE;
	}
	if (g_debugRenderer.vertexShader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(device, g_debugRenderer.vertexShader, nullptr);
		g_debugRenderer.vertexShader = VK_NULL_HANDLE;
	}
	if (g_debugRenderer.fragmentShader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(device, g_debugRenderer.fragmentShader, nullptr);
		g_debugRenderer.fragmentShader = VK_NULL_HANDLE;
	}
	g_debugRenderer.initialized = false;
}

/**
 * Helper function to render debug geometry (lines and triangles) from Jolt physics.
 * Vertex format: 7 floats per vertex (x, y, z, r, g, b, a)
 * 
 * IMPLEMENTATION GUIDE:
 * ====================
 * 
 * Phase 1: Buffer Creation (per-frame or cached)
 * -------
 * 1. Create vertex buffer for lines:
 *    - Size: lineVertices.size() * sizeof(float)
 *    - Usage: VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
 *    - Memory: Device local memory
 *    
 * 2. Create vertex buffer for triangles:
 *    - Size: triVertices.size() * sizeof(float)
 *    - Usage: VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
 *    - Memory: Device local memory
 * 
 * Phase 2: Data Transfer
 * -------
 * 1. Allocate staging buffer for lineVertices
 * 2. Allocate staging buffer for triVertices
 * 3. Record copy commands: vkCmdCopyBuffer to transfer staging -> device local
 * 4. Note: Can use getCommandBufferForDataTransfer() to batch transfers
 * 
 * Phase 3: Pipeline Creation
 * -------
 * 1. Create line pipeline:
 *    - VkPrimitiveTopology: VK_PRIMITIVE_TOPOLOGY_LINE_LIST
 *    - lineWidth: 1.0f (or use VK_EXT_line_rasterization for thicker lines)
 *    - Depth test: disabled
 *    - Blend: enabled (for transparency)
 * 
 * 2. Create triangle pipeline:
 *    - VkPrimitiveTopology: VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
 *    - rasterizationState.polygonMode: VK_POLYGON_MODE_FILL (solid) or VK_POLYGON_MODE_LINE (wireframe)
 *    - Depth test: disabled or enabled based on preference
 *    - Blend: enabled (for transparency)
 * 
 * Phase 4: Drawing
 * -------
 * 1. Get command buffer: vkGetCommandBuffer()
 * 2. Bind line pipeline and vertex buffer:
 *    - vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline)
 *    - vkCmdBindVertexBuffers(cmd, 0, 1, &lineBuffer, &offset)
 *    - vkCmdDraw(cmd, lineVertices.size(), 1, 0, 0)
 * 
 * 3. Bind triangle pipeline and vertex buffer:
 *    - vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, triPipeline)
 *    - vkCmdBindVertexBuffers(cmd, 0, 1, &triBuffer, &offset)
 *    - vkCmdDraw(cmd, triVertices.size(), 1, 0, 0)
 * 
 * Vertex Input Description:
 * --------
 * VkVertexInputBindingDescription binding{
 *     .binding = 0,
 *     .stride = 28,  // 7 floats * 4 bytes
 *     .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
 * };
 * 
 * VkVertexInputAttributeDescription attributes[2]{
 *     // Position (x, y, z)
 *     {
 *         .location = 0,
 *         .binding = 0,
 *         .format = VK_FORMAT_R32G32B32_SFLOAT,
 *         .offset = 0
 *     },
 *     // Color (r, g, b, a)
 *     {
 *         .location = 1,
 *         .binding = 0,
 *         .format = VK_FORMAT_R32G32B32A32_SFLOAT,
 *         .offset = 12
 *     }
 * };
 */

// Initialize debug rendering pipelines during context setup (outside render pass)
static void initializeDebugRendererPipelines(love::gfx::vulkan::Graphics* vulkanGraphics)
{
	printf("[DEBUG RENDERER] initializeDebugRendererPipelines called (initialized=%d, linePipeline=%p, triPipeline=%p)\n",
		g_debugRenderer.initialized, g_debugRenderer.linePipeline, g_debugRenderer.trianglePipeline);
	fflush(stdout);

	// Skip if already initialized or if pipelines already exist
	if (g_debugRenderer.initialized || 
		g_debugRenderer.linePipeline != VK_NULL_HANDLE ||
		g_debugRenderer.trianglePipeline != VK_NULL_HANDLE ||
		g_debugRenderer.debugRenderPass != VK_NULL_HANDLE) {
		printf("[DEBUG RENDERER] Pipelines already exist, skipping initialization\n");
		fflush(stdout);
		return;
	}
	
	if (!vulkanGraphics) {
		printf("[DEBUG RENDERER] Invalid graphics context for pipeline initialization\n");
		fflush(stdout);
		return;
	}

	printf("[DEBUG RENDERER] Graphics context valid, getting allocator and device\n");
	fflush(stdout);

	VmaAllocator allocator = vulkanGraphics->getAllocator();
	VkDevice device = vulkanGraphics->getDevice();
	
	printf("[DEBUG RENDERER] Allocator=%p, Device=%p\n", allocator, device);
	fflush(stdout);

	if (allocator == VK_NULL_HANDLE || device == VK_NULL_HANDLE) {
		printf("[DEBUG RENDERER] Invalid allocator or device for pipeline initialization\n");
		fflush(stdout);
		return;
	}

	printf("[DEBUG RENDERER] About to start shader creation\n");
	fflush(stdout);
	
	try {
		// Validate shader SPIR-V data
		if (sizeof(debugVertexShaderSPIRV) == 0 || sizeof(debugFragmentShaderSPIRV) == 0) {
			printf("[DEBUG RENDERER] Invalid shader SPIR-V data size\n");
			fflush(stdout);
			return;
		}
		
		printf("[DEBUG RENDERER] Shader SPIR-V sizes: vertex=%zu, fragment=%zu\n", 
			sizeof(debugVertexShaderSPIRV), sizeof(debugFragmentShaderSPIRV));
		fflush(stdout);
		
		// Create shader modules
		VkShaderModuleCreateInfo vertShaderInfo = {};
		vertShaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		vertShaderInfo.codeSize = sizeof(debugVertexShaderSPIRV);
		vertShaderInfo.pCode = debugVertexShaderSPIRV;
		
		// Verify SPIR-V magic number
		printf("[DEBUG RENDERER] Vertex shader magic: 0x%08X (expected 0x07230203)\n", debugVertexShaderSPIRV[0]);
		fflush(stdout);
		
		VkShaderModuleCreateInfo fragShaderInfo = {};
		fragShaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		fragShaderInfo.codeSize = sizeof(debugFragmentShaderSPIRV);
		fragShaderInfo.pCode = debugFragmentShaderSPIRV;
		
		printf("[DEBUG RENDERER] Fragment shader magic: 0x%08X (expected 0x07230203)\n", debugFragmentShaderSPIRV[0]);
		fflush(stdout);
		
		printf("[DEBUG RENDERER] Creating shader modules\n");
		fflush(stdout);
		VkResult vertResult = vkCreateShaderModule(device, &vertShaderInfo, nullptr, &g_debugRenderer.vertexShader);
		printf("[DEBUG RENDERER] Vertex shader module result: %d\n", vertResult);
		fflush(stdout);
		
		VkResult fragResult = vkCreateShaderModule(device, &fragShaderInfo, nullptr, &g_debugRenderer.fragmentShader);
		printf("[DEBUG RENDERER] Fragment shader module result: %d\n", fragResult);
		fflush(stdout);
		
		if (vertResult != VK_SUCCESS || fragResult != VK_SUCCESS) {
			printf("[DEBUG RENDERER] Failed to create shader modules: vert=%d frag=%d\n", vertResult, fragResult);
			fflush(stdout);
			g_debugRenderer.failed = true;
			return;
		}
		
		// Create pipeline layout (no descriptors needed for debug rendering)
		VkPipelineLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		
		if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &g_debugRenderer.pipelineLayout) != VK_SUCCESS) {
			printf("[DEBUG RENDERER] Failed to create pipeline layout\n");
			fflush(stdout);
			g_debugRenderer.failed = true;
			return;
		}
		
		// Obtain a compatible render pass from Graphics using the current configuration
		auto conf = vulkanGraphics->getCurrentRenderPassConfiguration();
		VkRenderPass compatibleRenderPass = vulkanGraphics->getCompatibleRenderPass(conf);
		if (compatibleRenderPass == VK_NULL_HANDLE) {
			printf("[DEBUG RENDERER] Failed to get compatible render pass from Graphics\n");
			fflush(stdout);
			g_debugRenderer.failed = true;
			return;
		}
		g_debugRenderer.debugRenderPass = compatibleRenderPass;
		
		g_debugRenderer.initialized = true;
		printf("[DEBUG RENDERER] Debug renderer initialized (shaders + layout ready, pipelines cannot be created during context_reset)\n");
		fflush(stdout);
		
	} catch (const std::exception& e) {
		printf("[DEBUG RENDERER] Exception during pipeline initialization: %s\n", e.what());
		fflush(stdout);
		g_debugRenderer.failed = true;
	}
}

// Create pipelines at a safe time (outside render pass, after full Vulkan initialization)
static void createDebugRendererPipelines(love::gfx::vulkan::Graphics* vulkanGraphics)
{
	// Skip if already created, failed, or not initialized
	if (g_debugRenderer.pipelinesCreated || g_debugRenderer.failed || !g_debugRenderer.initialized) {
		return;
	}
	
	// Skip if essential components not ready
	if (g_debugRenderer.vertexShader == VK_NULL_HANDLE || 
		g_debugRenderer.fragmentShader == VK_NULL_HANDLE ||
		g_debugRenderer.pipelineLayout == VK_NULL_HANDLE ||
		g_debugRenderer.debugRenderPass == VK_NULL_HANDLE) {
		return;
	}
	
	if (!vulkanGraphics) {
		return;
	}
	
	VkDevice device = vulkanGraphics->getDevice();
	if (device == VK_NULL_HANDLE) {
		return;
	}
	
	printf("[DEBUG RENDERER] Creating pipelines at safe time (outside render pass)\n");
	fflush(stdout);
	
	try {
		// Setup vertex input
		VkVertexInputBindingDescription bindingDesc = {};
		bindingDesc.binding = 0;
		bindingDesc.stride = 28;
		bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		
		VkVertexInputAttributeDescription attribs[2] = {};
		attribs[0].location = 0;
		attribs[0].binding = 0;
		attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribs[0].offset = 0;
		attribs[1].location = 1;
		attribs[1].binding = 0;
		attribs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attribs[1].offset = 12;
		
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
		vertexInputInfo.vertexAttributeDescriptionCount = 2;
		vertexInputInfo.pVertexAttributeDescriptions = attribs;
		
		VkPipelineShaderStageCreateInfo shaderStages[2] = {};
		shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shaderStages[0].module = g_debugRenderer.vertexShader;
		shaderStages[0].pName = "main";
		shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shaderStages[1].module = g_debugRenderer.fragmentShader;
		shaderStages[1].pName = "main";
		
		VkViewport dummyViewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
		VkRect2D dummyScissor = {{0, 0}, {1, 1}};
		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &dummyViewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &dummyScissor;
		
		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.cullMode = VK_CULL_MODE_NONE;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.lineWidth = 1.0f;
		rasterizer.depthBiasEnable = VK_FALSE;
		
		VkSampleCountFlagBits samples = vulkanGraphics->getCurrentMsaa();
		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = samples;
		
		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		
		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		
		VkPipelineDepthStencilStateCreateInfo depthStencil = {};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_FALSE;
		depthStencil.depthWriteEnable = VK_FALSE;
		depthStencil.stencilTestEnable = VK_FALSE;
		
		VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo dynamicState = {};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates = dynamicStates;
		
		// Create line pipeline
		VkPipelineInputAssemblyStateCreateInfo lineInputAssembly = {};
		lineInputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		lineInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		lineInputAssembly.primitiveRestartEnable = VK_FALSE;
		
		VkGraphicsPipelineCreateInfo linePipelineInfo = {};
		linePipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		linePipelineInfo.stageCount = 2;
		linePipelineInfo.pStages = shaderStages;
		linePipelineInfo.pVertexInputState = &vertexInputInfo;
		linePipelineInfo.pInputAssemblyState = &lineInputAssembly;
		linePipelineInfo.pViewportState = &viewportState;
		linePipelineInfo.pRasterizationState = &rasterizer;
		linePipelineInfo.pMultisampleState = &multisampling;
		linePipelineInfo.pDepthStencilState = &depthStencil;
		linePipelineInfo.pColorBlendState = &colorBlending;
		linePipelineInfo.pDynamicState = &dynamicState;
		linePipelineInfo.layout = g_debugRenderer.pipelineLayout;
		linePipelineInfo.renderPass = g_debugRenderer.debugRenderPass;
		linePipelineInfo.subpass = 0;
		linePipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		
		VkResult lineResult = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &linePipelineInfo, nullptr, &g_debugRenderer.linePipeline);
		if (lineResult != VK_SUCCESS) {
			printf("[DEBUG RENDERER] Failed to create line pipeline: %d\n", lineResult);
			fflush(stdout);
			g_debugRenderer.failed = true;
			return;
		}
		
		// Create triangle pipeline
		VkPipelineInputAssemblyStateCreateInfo triInputAssembly = {};
		triInputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		triInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		triInputAssembly.primitiveRestartEnable = VK_FALSE;
		
		VkGraphicsPipelineCreateInfo triPipelineInfo = linePipelineInfo;
		triPipelineInfo.pInputAssemblyState = &triInputAssembly;
		
		VkResult triResult = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &triPipelineInfo, nullptr, &g_debugRenderer.trianglePipeline);
		if (triResult != VK_SUCCESS) {
			printf("[DEBUG RENDERER] Failed to create triangle pipeline: %d\n", triResult);
			fflush(stdout);
			g_debugRenderer.failed = true;
			return;
		}
		
		g_debugRenderer.pipelinesCreated = true;
		printf("[DEBUG RENDERER] Pipelines created successfully\n");
		fflush(stdout);
		
	} catch (const std::exception& e) {
		printf("[DEBUG RENDERER] Exception during pipeline creation: %s\n", e.what());
		fflush(stdout);
		g_debugRenderer.failed = true;
	}
}

static void renderDebugGeometry(
	love::gfx::vulkan::Graphics* vulkanGraphics,
	const std::vector<float>& lineVertices, size_t lineCount,
	const std::vector<float>& triVertices, size_t triCount)
{
	printf("[DEBUG RENDERER] renderDebugGeometry called with lineCount=%zu, triCount=%zu\n", lineCount, triCount);
	fflush(stdout);

	// Skip entirely if debug renderer has failed
	if (g_debugRenderer.failed) {
		printf("[DEBUG RENDERER] Skipping: renderer failed\n");
		fflush(stdout);
		return;
	}

	printf("[DEBUG RENDERER] Passed failed check, initialized=%d\n", g_debugRenderer.initialized);
	fflush(stdout);

	// Skip if debug renderer failed or not initialized
	// Pipelines are created in initializeDebugRendererPipelines() called from context_reset()
	if (g_debugRenderer.failed || !g_debugRenderer.initialized) {
		printf("[DEBUG RENDERER] Skipping: not initialized (initialized=%d, failed=%d)\n", 
			g_debugRenderer.initialized, g_debugRenderer.failed);
		fflush(stdout);
		return;
	}

	printf("[DEBUG RENDERER] Passed initialized check, vulkanGraphics=%p, lineEmpty=%d, triEmpty=%d, lineVertices.size()=%zu\n", 
		vulkanGraphics, lineVertices.empty(), triVertices.empty(), lineVertices.size());
	fflush(stdout);

	// Early exit if no valid graphics context or no geometry
	if (!vulkanGraphics || (lineVertices.empty() && triVertices.empty())) {
		printf("[DEBUG RENDERER] Skipping: no graphics context or geometry (vulkanGraphics=%p, lineEmpty=%d, triEmpty=%d)\n", 
			vulkanGraphics, lineVertices.empty(), triVertices.empty());
		fflush(stdout);
		return;
	}
	
	printf("[DEBUG RENDERER] Passed geometry check, checking isInRenderPass\n");
	fflush(stdout);

	// Skip if not in render pass - can't record draw commands
	if (!vulkanGraphics->isInRenderPass()) {
		printf("[DEBUG RENDERER] Skipping: not in render pass\n");
		fflush(stdout);
		return;
	}

	printf("[DEBUG RENDERER] Passed render pass check, entering try block\n");
	fflush(stdout);

	try {
		VmaAllocator allocator = vulkanGraphics->getAllocator();
		VkDevice device = vulkanGraphics->getDevice();
		
		if (allocator == VK_NULL_HANDLE || device == VK_NULL_HANDLE) {
			printf("[DEBUG RENDERER] Invalid allocator or device\n");
			fflush(stdout);
			g_debugRenderer.failed = true;
			return;
		}

		printf("[DEBUG RENDERER] Allocator and device valid, starting vertex buffer creation\n");
		fflush(stdout);

		// Phase 1: Create/update vertex buffers
		if (!lineVertices.empty() && lineCount > 0) {
			printf("[DEBUG RENDERER] Creating line vertex buffer (size=%zu bytes)\n", lineVertices.size() * sizeof(float));
			fflush(stdout);
			size_t requiredSize = lineVertices.size() * sizeof(float);
			if (requiredSize == 0) {
				printf("[DEBUG RENDERER] Line vertex buffer has zero size\n");
				fflush(stdout);
				return;
			}
			
			// Recreate buffer if size changed
			printf("[DEBUG RENDERER] Check buffer: lineVertexBuffer=%p, bufferSize=%zu, requiredSize=%zu\n", 
				g_debugRenderer.lineVertexBuffer, g_debugRenderer.lineVertexBufferSize, requiredSize);
			fflush(stdout);
			if (g_debugRenderer.lineVertexBuffer == VK_NULL_HANDLE || requiredSize > g_debugRenderer.lineVertexBufferSize) {
				printf("[DEBUG RENDERER] Recreating line vertex buffer\n");
				fflush(stdout);
				if (g_debugRenderer.lineVertexBuffer != VK_NULL_HANDLE) {
					vmaDestroyBuffer(allocator, g_debugRenderer.lineVertexBuffer, g_debugRenderer.lineVertexAllocation);
					g_debugRenderer.lineVertexBuffer = VK_NULL_HANDLE;
					g_debugRenderer.lineVertexAllocation = nullptr;
				}
				
				VkBufferCreateInfo bufferInfo = {};
				bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				bufferInfo.size = requiredSize;
				bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
				bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				
				VmaAllocationCreateInfo allocInfo = {};
				allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
				allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
				
				VkResult result = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &g_debugRenderer.lineVertexBuffer, 
					&g_debugRenderer.lineVertexAllocation, nullptr);
				if (result != VK_SUCCESS) {
					printf("[DEBUG RENDERER] Failed to create line vertex buffer: %d\n", result);
					fflush(stdout);
					g_debugRenderer.lineVertexBuffer = VK_NULL_HANDLE;
					return;
				}
				printf("[DEBUG RENDERER] Line vertex buffer created successfully\n");
				fflush(stdout);
				g_debugRenderer.lineVertexBufferSize = requiredSize;
			}
			
			// Upload vertex data
			printf("[DEBUG RENDERER] Uploading line vertex data (%zu bytes)\n", requiredSize);
			fflush(stdout);
			if (g_debugRenderer.lineVertexBuffer != VK_NULL_HANDLE && g_debugRenderer.lineVertexAllocation != nullptr) {
				void* mappedData = nullptr;
				VkResult mapResult = vmaMapMemory(allocator, g_debugRenderer.lineVertexAllocation, &mappedData);
				if (mapResult == VK_SUCCESS && mappedData != nullptr) {
					memcpy(mappedData, lineVertices.data(), requiredSize);
					vmaUnmapMemory(allocator, g_debugRenderer.lineVertexAllocation);
					printf("[DEBUG RENDERER] Line vertex data uploaded successfully\n");
					fflush(stdout);
				} else {
					printf("[DEBUG RENDERER] Failed to map line vertex buffer memory: %d\n", mapResult);
					fflush(stdout);
				}
			}
		}

		if (!triVertices.empty() && triCount > 0) {
			size_t requiredSize = triVertices.size() * sizeof(float);
			if (requiredSize == 0) {
				printf("[DEBUG RENDERER] Triangle vertex buffer has zero size\n");
				fflush(stdout);
				return;
			}
			
			// Recreate buffer if size changed
			if (g_debugRenderer.triVertexBuffer == VK_NULL_HANDLE || requiredSize > g_debugRenderer.triVertexBufferSize) {
				if (g_debugRenderer.triVertexBuffer != VK_NULL_HANDLE) {
					vmaDestroyBuffer(allocator, g_debugRenderer.triVertexBuffer, g_debugRenderer.triVertexAllocation);
					g_debugRenderer.triVertexBuffer = VK_NULL_HANDLE;
					g_debugRenderer.triVertexAllocation = nullptr;
				}
				
				VkBufferCreateInfo bufferInfo = {};
				bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				bufferInfo.size = requiredSize;
				bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
				bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				
				VmaAllocationCreateInfo allocInfo = {};
				allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
				allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
				
				VkResult result = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &g_debugRenderer.triVertexBuffer,
					&g_debugRenderer.triVertexAllocation, nullptr);
				if (result != VK_SUCCESS) {
					printf("[DEBUG RENDERER] Failed to create triangle vertex buffer: %d\n", result);
					fflush(stdout);
					g_debugRenderer.triVertexBuffer = VK_NULL_HANDLE;
					return;
				}
				g_debugRenderer.triVertexBufferSize = requiredSize;
			}
			
			// Upload vertex data
			if (g_debugRenderer.triVertexBuffer != VK_NULL_HANDLE && g_debugRenderer.triVertexAllocation != nullptr) {
				void* mappedData = nullptr;
				VkResult mapResult = vmaMapMemory(allocator, g_debugRenderer.triVertexAllocation, &mappedData);
				if (mapResult == VK_SUCCESS && mappedData != nullptr) {
					memcpy(mappedData, triVertices.data(), requiredSize);
					vmaUnmapMemory(allocator, g_debugRenderer.triVertexAllocation);
				} else {
					printf("[DEBUG RENDERER] Failed to map triangle vertex buffer memory: %d\n", mapResult);
					fflush(stdout);
				}
			}
		}
		
		printf("[DEBUG RENDERER] Vertex buffers ready, checking pipelines (initialized=%d, pipelinesCreated=%d, linePipeline=%p)\n", 
			g_debugRenderer.initialized, g_debugRenderer.pipelinesCreated, g_debugRenderer.linePipeline);
		fflush(stdout);
		
		// Skip drawing if pipelines aren't created yet
		if (!g_debugRenderer.pipelinesCreated || g_debugRenderer.linePipeline == VK_NULL_HANDLE) {
			printf("[DEBUG RENDERER] Pipelines not ready, skipping draw (pipelinesCreated=%d, linePipeline=%p)\n", 
				g_debugRenderer.pipelinesCreated, g_debugRenderer.linePipeline);
			fflush(stdout);
			return;
		}
		
		// Phase 3: Record draw commands using the current frame's command buffer
		// Use the same command buffer that's active in the current render pass
		auto commandBuffers = vulkanGraphics->getCommandBuffersForDataTransfer();
		size_t currentFrame = vulkanGraphics->getCurrentFrame();
		if (currentFrame >= commandBuffers.size()) {
			printf("[DEBUG RENDERER] Invalid frame index: %zu >= %zu\n", currentFrame, commandBuffers.size());
			fflush(stdout);
			return;
		}
		VkCommandBuffer cmd = commandBuffers.at(currentFrame);
		if (cmd == VK_NULL_HANDLE) {
			printf("[DEBUG RENDERER] Invalid command buffer\n");
			fflush(stdout);
			return;
		}
		
		if (cmd != VK_NULL_HANDLE && g_debugRenderer.initialized) {
			printf("[DEBUG RENDERER] About to draw - cmd=%p, linePipeline=%p, triPipeline=%p, lineBuffer=%p, triBuffer=%p\n",
				cmd, g_debugRenderer.linePipeline, g_debugRenderer.trianglePipeline, 
				g_debugRenderer.lineVertexBuffer, g_debugRenderer.triVertexBuffer);
			fflush(stdout);
			try {
				// Set viewport and scissor
				VkViewport viewport = {};
				viewport.x = 0.0f;
				viewport.y = 0.0f;
				viewport.width = (float)vulkanGraphics->getWidth();
				viewport.height = (float)vulkanGraphics->getHeight();
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;
				vkCmdSetViewport(cmd, 0, 1, &viewport);
				
				VkRect2D scissor = {};
				scissor.offset = {0, 0};
				scissor.extent = {(uint32_t)vulkanGraphics->getWidth(), (uint32_t)vulkanGraphics->getHeight()};
				vkCmdSetScissor(cmd, 0, 1, &scissor);
				
				// Draw lines with debug pipeline
				if (g_debugRenderer.linePipeline != VK_NULL_HANDLE && g_debugRenderer.lineVertexBuffer != VK_NULL_HANDLE && !lineVertices.empty()) {
					VkDeviceSize offset = 0;
					vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_debugRenderer.linePipeline);
					vkCmdBindVertexBuffers(cmd, 0, 1, &g_debugRenderer.lineVertexBuffer, &offset);
					uint32_t vertexCount = lineVertices.size() / 7; // 7 floats per vertex
					if (vertexCount > 0) {
						printf("[DEBUG RENDERER] Drawing %u line vertices\n", vertexCount);
						fflush(stdout);
						vkCmdDraw(cmd, vertexCount, 1, 0, 0);
					}
				}
				
				// Draw triangles with debug pipeline
				if (g_debugRenderer.trianglePipeline != VK_NULL_HANDLE && g_debugRenderer.triVertexBuffer != VK_NULL_HANDLE && !triVertices.empty()) {
					VkDeviceSize offset = 0;
					vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_debugRenderer.trianglePipeline);
					vkCmdBindVertexBuffers(cmd, 0, 1, &g_debugRenderer.triVertexBuffer, &offset);
					uint32_t vertexCount = triVertices.size() / 7; // 7 floats per vertex
					if (vertexCount > 0) {
						printf("[DEBUG RENDERER] Drawing %u triangle vertices\n", vertexCount);
						fflush(stdout);
						vkCmdDraw(cmd, vertexCount, 1, 0, 0);
					}
				}
			} catch (const std::exception& drawEx) {
				printf("[DEBUG RENDERER] Error during draw: %s\n", drawEx.what());
				fflush(stdout);
				g_debugRenderer.failed = true;
			}
		}
		
	} catch (const std::exception& e) {
		printf("[DEBUG RENDERER] Error: %s\n", e.what());
		fflush(stdout);
		g_debugRenderer.failed = true;
	}
}
#endif

// This is needed to allow SDL-libretro to compile.
// @see SDL_LIBRETROaudio.c:37
retro_audio_sample_t audio_cb;
char *home_directory;
struct retro_vfs_interface *vfs_interface;

void retro_set_video_refresh(retro_video_refresh_t cb) {
	video_cb = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb) {
	ChaiLove::getInstance()->sound.audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
	ChaiLove::getInstance()->sound.audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb) {
	ChaiLove::input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb) {
	ChaiLove::input_state_cb = cb;
}

static void fallback_log(enum retro_log_level level,
			 const char *fmt, ...) {
    va_list args;

    (void) level;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

#ifdef __cplusplus
extern "C" {
#endif

void libretro_audio_cb(int16_t left, int16_t right) {
	// Nothing, since we're using libretro-common.
	// audio_cb(left, right);
}

/**
 * libretro-sdl callback; Send through the input state.
 */
short int libretro_input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
	return ChaiLove::input_state_cb(port, device, index, id);
}
#ifdef __cplusplus
}
#endif

/**
 * libretro callback; Sets up the environment based on the system variables.
 */
void retro_set_environment(retro_environment_t cb) {
	// Set the environment callback.
	ChaiLove::environ_cb = cb;

	// The core requires content.
	bool no_rom = false;
	cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_rom);

	// Configure the core options.
	libretro_set_core_options(cb);

	struct retro_vfs_interface_info vfs_interface_info;
	vfs_interface_info.required_interface_version = DIRENT_REQUIRED_VFS_VERSION;
	vfs_interface_info.iface = NULL;
	if (cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_interface_info)) {
		vfs_interface = vfs_interface_info.iface;
		filestream_vfs_init(&vfs_interface_info);
		dirent_vfs_init(&vfs_interface_info);
	}

	struct retro_log_callback log;

	if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log)) {
		log_cb = log.log;
	}
}

/**
 * libretro callback; Updates the core option variables.
 */
static void update_variables(void) {
	ChaiLove* app = ChaiLove::getInstance();
	app->system.updateVariables(app->config);
}

#ifdef __cplusplus
extern "C" {
#endif
/**
 * libretro callback; Load the labels for the input buttons.
 */
void init_descriptors() {
	struct retro_input_descriptor desc[] = {
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "Left Shoulder" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "Right Shoulder" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },

		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "Left Shoulder" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "Right Shoulder" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },

		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "Left Shoulder" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "Right Shoulder" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },

		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "Left Shoulder" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "Right Shoulder" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },

		{ 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left" },
		{ 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
		{ 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down" },
		{ 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
		{ 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
		{ 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
		{ 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
		{ 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
		{ 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "Left Shoulder" },
		{ 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "Right Shoulder" },
		{ 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
		{ 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
		{ 0 },
	};

	ChaiLove::environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
}
#ifdef __cplusplus
}
#endif

/**
 * libretro callback; Retrieve information about the core.
 */
void retro_get_system_info(struct retro_system_info *info) {
	memset(info, 0, sizeof(*info));

	// The name of the core.
	info->library_name = "ChaiLove";
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif

	// The ChaiLove version.
	info->library_version  = CHAILOVE_VERSION_STRING GIT_VERSION;

	// When loading a game, request the full path to the game.
	info->need_fullpath = true;

	// File extensions that are used.
	info->valid_extensions = "chai|chailove";

	// Do not extract .zip files.
	info->block_extract = true;
}

/**
 * libretro callback; Set the audio/video settings.
 */
void retro_get_system_av_info(struct retro_system_av_info *info) {
	// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] retro_get_system_av_info" << std::endl;
	if (!ChaiLove::hasInstance()) {
		return;
	}

	unsigned int width = 3840;
	unsigned int height = 2160;

	// GLint dims[2];
    // glGetIntegerv(GL_MAX_VIEWPORT_DIMS, dims);
	// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] GL_MAX_VIEWPORT_DIMS: " << dims[0] << "x" << dims[1] << std::endl;
    // width = dims[0];
    // height = dims[1];

	// ChaiLove* app = ChaiLove::getInstance();
	// app->chai_gfx.width = width;
	// app->chai_gfx.height = height;
	// if (app != NULL) {
	// 	width = app->config.window.width;
	// 	height = app->config.window.height;
	// }

	info->geometry.base_width   = 1920;
	info->geometry.base_height  = 1080;
	info->geometry.max_width    = width;
	info->geometry.max_height   = height;
	info->geometry.aspect_ratio = static_cast<float>(width) / static_cast<float>(height);

	info->timing.fps = 60.0;
	info->timing.sample_rate = 44100.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device) {
	LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] retro_set_controller_port_device" << std::endl;
	(void)port;
	(void)device;
}

/**
 * libretro callback; Return the amount of bytes required to save a state.
 */
size_t retro_serialize_size(void) {
	// Save states will be 8 KB.
	return 8192;
}

/**
 * libretro callback; Serialize the current state to save a slot.
 */
bool retro_serialize(void *data, size_t size) {
	if (!ChaiLove::hasInstance()) {
		return false;
	}

	// Ask ChaiLove for save data.
	ChaiLove* app = ChaiLove::getInstance();
	std::string state = app->savestate();
	if (state.empty()) {
		return false;
	}

	// Encode the JSON state data. Disabled for speed.
	// state = app->data.encode("string", "base64", state);
	// state = app->data.compress(state);

	// Save the information to the state data.
	std::copy(state.begin(), state.end(), reinterpret_cast<char*>(data));
	return true;
}

/**
 * libretro callback; Unserialize the given data and load the state.
 */
bool retro_unserialize(const void *data, size_t size) {
	if (!ChaiLove::hasInstance() || size <= 0) {
		return false;
	}

	// Create a string stream from the data.
	std::stringstream ss(std::string(
		reinterpret_cast<const char*>(data),
		reinterpret_cast<const char*>(data) + size));

	// Port the string stream to a straight string.
	std::string loadData = ss.str();

	// Pass the string to the script.
	ChaiLove* app = ChaiLove::getInstance();

	// Decompress the state data. Disabled for speed.
	// loadData = app->data.decode("string", "base64", loadData);
	// loadData = app->data.decompress(loadData);

	// Finally, load the string.
	return app->loadstate(loadData);
}

/**
 * libretro callback; Reset the enabled cheats.
 */
void retro_cheat_reset(void) {
	if (ChaiLove::hasInstance()) {
		ChaiLove::getInstance()->cheatreset();
	}
}

/**
 * libretro callback; Set the given cheat.
 */
void retro_cheat_set(unsigned index, bool enabled, const char *code) {
	if (ChaiLove::hasInstance()) {
		std::string codeString(code);
		ChaiLove::getInstance()->cheatset(index, enabled, codeString);
	}
}

/**
 * libretro callback; Step the core forwards a step.
 */
void frame_time_cb(retro_usec_t usec) {
	if (ChaiLove::hasInstance()) {
		float delta = (float)usec / 1000000.0f;
		ChaiLove::getInstance()->timer.step(delta);
	}
}

/**
 * libretro callback; Step the audio forwards a step.
 */
void retro_audio_cb() {
	// Update the sound system.
	ChaiLove::getInstance()->sound.update();
}

/**
 * libretro callback; Set the current state of the audio.
 */
void audio_set_state(bool enabled) {
	// TODO(RobLoach): Act on whether or not audio is enabled/disabled?
	// LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] audio_set_state(" << (enabled ? "true" : "false") << ")" << std::endl;
}

/**
 * libretro callback; Load the given game.
 */
bool retro_load_game(const struct retro_game_info *info) {
	// Update the core options.
	update_variables();

	// Update the input button descriptions.
	init_descriptors();

	// Set the frame rate callback.
	struct retro_frame_time_callback frame_cb = { frame_time_cb, 1000000 / 60 };
	ChaiLove::environ_cb(RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK, &frame_cb);

	// Set the audio callback.
	struct retro_audio_callback retro_audio = { retro_audio_cb, audio_set_state };
	ChaiLove::environ_cb(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK, &retro_audio);

	// Find the game path.
	std::string gamePath(info ? info->path : "");
	if (gamePath == ".") {
		gamePath = "main.chai";
	}
	void* data = NULL;
	if (info != NULL) {
		data = (void*)info->data;
	}
	return ChaiLove::getInstance()->load(gamePath, data);
}

/**
 * libretro callback; Loads the given special game.
 */
bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info) {
	LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] retro_load_game_special" << std::endl;
	return retro_load_game(info);
}

/**
 * libretro callback; Unload the current game.
 */
void retro_unload_game(void) {
	LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] retro_unload_game()" << std::endl;

	// Invoke the quit event.
	if (ChaiLove::hasInstance()) {
		ChaiLove::getInstance()->event.m_shouldclose = true;
	}
}

/**
 * libretro callback; Retrieve the active region.
 */
unsigned retro_get_region(void) {
	return RETRO_REGION_NTSC;
}

/**
 * libretro callback; Get the libretro API version.
 */
unsigned retro_api_version(void) {
	return RETRO_API_VERSION;
}

/**
 * libretro callback; Get the given memory ID.
 */
void *retro_get_memory_data(unsigned id) {
	return NULL;
}

/**
 * libretro callback; Get the size of the given memory ID.
 */
size_t retro_get_memory_size(unsigned id) {
	return 0;
}


static void context_reset(void)
{

	if (!ChaiLove::environ_cb(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE, (void**)&vulkan) || !vulkan)
	{
		fprintf(stderr, "Failed to get HW rendering interface!\n");
		return;
	}

	ChaiLove::getInstance()->chai_gfx.hw_render = hw_render;
	// ChaiLove::getInstance()->chai_gfx.setVulkanInterface(reinterpret_cast<const love::retro_hw_render_interface_vulkan*>(vulkan));
	ChaiLove::getInstance()->chai_gfx.setVulkanInterface(vulkan);
	// ChaiLove::getInstance()->chai_gfx.FRAMEBUFFER = RARCH_GL_FRAMEBUFFER;
	// ChaiLove::getInstance()->chai_gfx.COLORATTACH = RARCH_GL_COLOR_ATTACHMENT0;
	// printf("context_reset\n");
	// printf("FRAMEBUFFER: %d\n", RARCH_GL_FRAMEBUFFER);
	// printf("hw_render: %d\n", hw_render.get_current_framebuffer());
	ChaiLove::getInstance()->chai_gfx.init();

	// DISABLED: clearAllDescriptors() - may be clearing active descriptors
	// love::gfx::vulkan::Shader::clearAllDescriptors();
	
	ChaiLove::getInstance()->chai_collisions.clearWorlds();
	ChaiLove::getInstance()->chai_collisions.init(0);
	
	#ifdef JPH_DEBUG_RENDERER
	// NOTE: Debug rendering now uses app->graphics.line() instead of custom Vulkan pipelines
	// No initialization needed - just draw directly in retro_run()
	#endif
}

static void context_destroy(void)
{
	#ifdef JPH_DEBUG_RENDERER
	// Clean up debug renderer resources
	auto* vulkanGraphics = static_cast<love::gfx::vulkan::Graphics*>(ChaiLove::getInstance()->chai_gfx.instance);
	if (vulkanGraphics) {
		cleanupDebugRenderer(vulkanGraphics->getAllocator(), vulkanGraphics->getDevice());
	}
	#endif
	
	ChaiLove::getInstance()->chai_gfx.destroy();
	ChaiLove::getInstance()->chai_collisions.destroy();
}

static const VkApplicationInfo *get_application_info(void)
{
   static const VkApplicationInfo info = {
      VK_STRUCTURE_TYPE_APPLICATION_INFO,
      NULL,
      "libretro-test-vulkan",
      0,
      "libretro-test-vulkan",
      0,
      VK_MAKE_VERSION(1, 0, 18),
   };
   return &info;
}

static bool retro_init_hw_context(void)
{
    hw_render.context_type = RETRO_HW_CONTEXT_VULKAN;
	hw_render.version_major = VK_MAKE_VERSION(1, 0, 18);
	hw_render.version_minor = 0;
	hw_render.context_reset = context_reset;
	hw_render.context_destroy = context_destroy;
	hw_render.cache_context = true;
	if (!ChaiLove::environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
		return false;

	static const struct retro_hw_render_context_negotiation_interface_vulkan iface = {
		RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN,
		RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN_VERSION,

		get_application_info,
		NULL,
	};

	ChaiLove::environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE, (void*)&iface);

   return true;
}

/**
 * libretro callback; Initialize the core.
 */
void retro_init(void) {

	// MemPlumber::start();

	// Pixel Format
	enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
	if (!ChaiLove::environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
		LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] Pixel format XRGB8888 not supported by platform, cannot use." << std::endl;
	}

	if (!retro_init_hw_context())
	{
		LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] HW Context could not be initialized." << std::endl;
	}

	const char *content_dir = NULL;

	ChaiLove::environ_cb(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, &content_dir);

	if (!home_directory && !content_dir)
		content_dir = "/";

	if (content_dir) {
		size_t l = strlen(content_dir);
		home_directory = (char *) malloc(l + 1);
		if (home_directory)
			memcpy(home_directory, content_dir, l + 1);
	}
}

/**
 * libretro callback; Deinitialize the core.
 */
void retro_deinit(void) {
	LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] retro_deinit()" << std::endl;
	ChaiLove::destroy();
}

/**
 * libretro callback; The frontend requested to reset the game.
 */
void retro_reset(void) {
	LibretroLog::log(RETRO_LOG_INFO) << "[ChaiLove] retro_reset()" << std::endl;
	if (ChaiLove::hasInstance()) {
		ChaiLove::getInstance()->reset();
	}
}




int runCount = 0;

/**
 * libretro callback; Run a game loop in the core.
 */
void retro_run(void) {
	// Ensure there is a game running.
	if (!ChaiLove::hasInstance()) {
		return;
	}

	// Check if the game should be closed.
	ChaiLove* app = ChaiLove::getInstance();
	if (app->event.m_shouldclose) {
		return;
	}
	
	// Log VMA statistics every 60 frames to track memory usage
	// WARNING: This causes RenderDoc to lock up at frame 58-60 due to _heapwalk() and memory operations
	static int statsFrameCounter = 0;
	static uint32_t lastAllocCount = 0;
	static SIZE_T lastSystemRAM = 0;
	if (++statsFrameCounter >= 60)
	{
		statsFrameCounter = 0;
		
		auto& cg = ChaiLove::getInstance()->chai_gfx;
		auto* vulkanGraphics = static_cast<love::gfx::vulkan::Graphics*>(cg.instance);
		
		VmaTotalStatistics stats;
		vmaCalculateStatistics(vulkanGraphics->getAllocator(), &stats);
		
		uint32_t allocDelta = stats.total.statistics.allocationCount - lastAllocCount;
		
		// Get detailed memory usage breakdown
		PROCESS_MEMORY_COUNTERS_EX pmc;
		GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
		SIZE_T systemRAM = pmc.PrivateUsage / (1024 * 1024);
		SIZE_T ramDelta = systemRAM - lastSystemRAM;
		
		// Track heap allocations vs committed memory to identify fragmentation
		// DISABLED: _heapwalk() causes freeze at frame 58-60
		// _HEAPINFO hinfo;
		// int heapstatus;
		// hinfo._pentry = NULL;
		size_t totalHeapUsed = 0;
		size_t totalHeapCommitted = 0;
		// while ((heapstatus = _heapwalk(&hinfo)) == _HEAPOK) {
		// 	if (hinfo._useflag == _USEDENTRY) {
		// 		totalHeapUsed += hinfo._size;
		// 	}
		// 	totalHeapCommitted += hinfo._size;
		// }
		size_t heapWaste = 0; // totalHeapCommitted - totalHeapUsed;
		
		SIZE_T workingSet = pmc.WorkingSetSize / (1024 * 1024);
		SIZE_T privateBytes = pmc.PrivateUsage / (1024 * 1024);
		SIZE_T pageFaults = pmc.PageFaultCount;
		
		static SIZE_T lastPageFaults = 0;
		SIZE_T pageFaultDelta = pageFaults - lastPageFaults;
		lastPageFaults = pageFaults;
		
		size_t stagingSize = vulkanGraphics->getStagingBufferPoolSize();
		static size_t lastStagingSize = 0;
		size_t stagingDelta = stagingSize > lastStagingSize ? stagingSize - lastStagingSize : 0;
		
		// PROOF: Calculate all tracked memory sources vs total RAM
		size_t gpuMemory = stats.total.statistics.allocationBytes / (1024 * 1024);
		size_t heapMemory = totalHeapUsed / (1024 * 1024);
		size_t trackedTotal = gpuMemory + heapMemory + stagingSize;
		size_t untracked = systemRAM > trackedTotal ? systemRAM - trackedTotal : 0;
		
		std::printf("[MEMORY] GPU: %llu MB | Staging: %zu (+%zu) | RAM: %llu MB (+%llu MB/min)\n",
			gpuMemory,
			stagingSize,
			stagingDelta,
			systemRAM,
			ramDelta);
		// std::printf("[HEAP] Used: %zu MB | Waste: %zu MB | WorkingSet: %llu MB | PageFaults: +%llu\n",
		// 	totalHeapUsed / (1024 * 1024),
		// 	heapWaste / (1024 * 1024),
		// 	workingSet,
		// 	pageFaultDelta);
		
		// Check if descriptor fix reduced leak
		// if (ramDelta > 100) {
		// 	std::printf("[LEAK ANALYSIS] Still leaking %llu MB/min after descriptor fix - investigating other sources\n", ramDelta);
		// } else {
		// 	std::printf("[LEAK FIXED] Memory stable at +%llu MB/min - descriptor fix successful!\n", ramDelta);
		// }
		
		// PROOF: Show the leak is in untracked memory (driver private allocations)
		// static size_t lastUntracked = 0;
		// size_t untrackedDelta = untracked > lastUntracked ? untracked - lastUntracked : 0;
		// std::printf("[LEAK PROOF] Tracked: %zu MB (GPU+Heap+Staging) | Untracked: %zu MB (+%zu) <- DRIVER PRIVATE\n",
		// 	trackedTotal,
		// 	untracked,
		// 	untrackedDelta);
		// lastUntracked = untracked;
		
		fflush(stdout);
		
		lastStagingSize = stagingSize;
		
		lastAllocCount = stats.total.statistics.allocationCount;
		lastSystemRAM = systemRAM;
		
		// AGGRESSIVE: Flush every 60 frames to prevent driver memory accumulation
		// The leak is in GPU driver private memory, not C++ heap (MemPlumber shows stable counts)
		vulkanGraphics->flushBatchedDraws();
		
		// AMD INTEGRATED GPU DRIVER BUG: Persistent 77-78 MB/min leak in Vulkan driver private memory
		// - CONFIRMED: Leak occurs on AMD integrated graphics, NOT on NVIDIA discrete GPUs
		// - Heap tracking shows 13 MB stable (NOT heap fragmentation)
		// - VMA shows GPU 536 MB stable (NOT GPU memory leak)
		// - ~26,000 page faults/min indicate driver allocating new virtual pages
		// - Working set trims from 1200 MB to 94 MB (memory CAN be released)
		// - Conclusion: AMD integrated graphics Vulkan driver bug - private allocations outside our control
		//
		// WORKAROUNDS IMPLEMENTED:
		// 1. DONE: Reduced recycleCommandPool() frequency to every 600 frames (10 seconds)
		// 2. DONE: Added vkQueueWaitIdle() before vkDeviceWaitIdle() for better queue synchronization
		// 3. DONE: Explicitly free descriptor sets before resetting pool (vkFreeDescriptorSets)
		// 4. DONE: Avoid pipeline cache recreation - only destroy/recreate every 3600 frames (60 seconds)
		// 5. DONE: Using VK_AMD_memory_overallocation_behavior extension with DISALLOWED mode
		// 6. TODO: Report bug to AMD with minimal repro case
		// 7. DONE: Periodic staging buffer cleanup to prevent pool accumulation
		
		static int recycleCounter = 0;
		
		// MEMORY LEAK FIX: Recycle less frequently and WITHOUT pipeline cache destruction
		// The leak was caused by destroying/recreating pipeline cache every 60 frames
		// Now recycle command pool every 120 frames, but keep pipeline cache intact
		if (++recycleCounter >= 120) {  // Reduced frequency: 120 frames = 2 seconds
			recycleCounter = 0;
			
			// Recycle command pool WITHOUT destroying pipeline cache (false parameter)
			// This prevents frequent pipeline recreation that causes AMD driver leaks
			vulkanGraphics->recycleCommandPool(false);
			
			// WORKAROUND: Aggressive Windows memory management
			HANDLE hProcess = GetCurrentProcess();
			
			// 1. Trim working set to force driver pages to decommit
			SetProcessWorkingSetSize(hProcess, (SIZE_T)-1, (SIZE_T)-1);
			EmptyWorkingSet(hProcess);
			
			// 2. Force heap compaction (helps with CRT overhead, not the main leak)
			_heapmin();
			
			// 3. Force CRT to release cached memory
			_aligned_free(_aligned_malloc(1, 16));
			
			// printf("[DRIVER WORKAROUND] Vulkan resources reset + working set trim (known driver leak)\n");
			// fflush(stdout);
		}
	}

	bool updated = false;
	if (ChaiLove::environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
		update_variables();
	}

	// PERF: Add frame timing to identify bottleneck
	static int perfCounter = 0;
	static int frameNumber = 0;
	auto frameStart = std::chrono::high_resolution_clock::now();
	
	frameNumber++;
	
	// Update the game.
	auto updateStart = std::chrono::high_resolution_clock::now();
	app->update();
	auto updateEnd = std::chrono::high_resolution_clock::now();

	// Clear the color and depth buffers
    // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set the clear color (optional, if you want to change the background color)
    // glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	auto& cg = ChaiLove::getInstance()->chai_gfx;
	auto* vulkanGraphics = static_cast<love::gfx::vulkan::Graphics*>(cg.instance);
	
	// START THE RENDER PASS BEFORE DRAWING
    if (!app->event.m_pauserendering) {
        // love::gfx::OptionalColorD clearcolor;
        // clearcolor = love::ColorD(1.0, 0.0, 0.0, 1.0); // Set the clear color to black with full opacity
        // love::OptionalInt clearstencil(0);
        // love::OptionalDouble cleardepth(1.0);
        // cg.instance->clear(clearcolor, clearstencil, cleardepth);
		// std::printf("[CHAILOVE DEBUG] LIBRETRO: Started render pass with green clear color before app->draw()\n");
    }	

	// Render the game.
	auto drawStart = std::chrono::high_resolution_clock::now();
	app->draw();
	auto drawEnd = std::chrono::high_resolution_clock::now();

	#ifdef JPH_DEBUG_RENDERER
	#if ENABLE_DEBUG_GEOMETRY_RENDERING
	// Render physics debug geometry as polylines
	try {
		// Get line vertices from the default world group (0)
		auto lineVertices = app->chai_collisions.getDebugRendererLineVertices(0);
		size_t lineCount = app->chai_collisions.getDebugRendererLineCount(0);
		
		// Debug output to verify geometry is being collected
		if (lineCount > 0 && runCount % 60 == 0) {
			printf("[DEBUG RENDERER] Drawing %zu line segments as polylines\n", lineCount);
			fflush(stdout);
		}
		
		// Draw lines as polylines (not points!)
		if (lineCount > 0 && lineVertices.size() >= lineCount * 14 && vulkanGraphics) {
			// Save the current state
			vulkanGraphics->push();

			// Compute world-space bounds for an orthographic projection
			float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
			float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;
			for (size_t i = 0; i < lineCount; i++) {
				size_t idx = i * 14;
				float x1 = lineVertices[idx + 0];
				float y1 = lineVertices[idx + 1];
				float z1 = lineVertices[idx + 2];
				float x2 = lineVertices[idx + 7];
				float y2 = lineVertices[idx + 8];
				float z2 = lineVertices[idx + 9];
				minX = std::min({minX, x1, x2});
				minY = std::min({minY, y1, y2});
				minZ = std::min({minZ, z1, z2});
				maxX = std::max({maxX, x1, x2});
				maxY = std::max({maxY, y1, y2});
				maxZ = std::max({maxZ, z1, z2});
			}

			// Debug output
			if (runCount % 60 == 0) {
				printf("[DEBUG] World bounds: X[%.2f, %.2f] Y[%.2f, %.2f] Z[%.2f, %.2f]\n",
					minX, maxX, minY, maxY, minZ, maxZ);
			}

			// Expand bounds slightly to avoid clipping
			float margin = 0.1f; // 10%
			float width = maxX - minX;
			float height = maxY - minY;
			float depth = maxZ - minZ;
			if (width <= 0.0f) width = 1.0f;
			if (height <= 0.0f) height = 1.0f;
			if (depth <= 0.0f) depth = 1.0f;
			float padX = width * margin;
			float padY = height * margin;
			float padZ = depth * margin;
			
			// Create orthographic projection for the physics world bounds
			love::Matrix4 physicsOrtho = love::Matrix4::ortho(minX - padX, maxX + padX,
				maxY + padY, minY - padY, minZ - padZ, maxZ + padZ);
			vulkanGraphics->setProjection(physicsOrtho);
			vulkanGraphics->replaceTransform(love::Matrix4());

			// Disable depth testing and writing for debug overlay
			vulkanGraphics->setDepthMode(love::gfx::COMPARE_ALWAYS, false);
			// Use thin, rough lines for debug visualization
			vulkanGraphics->setLineWidth(0.25f);
			vulkanGraphics->setLineStyle(love::gfx::Graphics::LINE_ROUGH);
			vulkanGraphics->setLineJoin(love::gfx::Graphics::LINE_JOIN_NONE);

			// Draw each line segment
			for (size_t i = 0; i < lineCount; i++) {
				size_t idx = i * 14;
				float x1 = lineVertices[idx + 0];
				float y1 = lineVertices[idx + 1];
				float a1 = lineVertices[idx + 6] / 255.0f;
				
				float x2 = lineVertices[idx + 7];
				float y2 = lineVertices[idx + 8];
				
				// Vary color per line (ignore source color to avoid black)
				float t = (float)(i % 12) / 11.0f;
				float vr = 0.2f + 0.8f * (1.0f - t);
				float vg = 0.2f + 0.8f * t;
				float vb = 0.4f + 0.4f * ((i % 3) / 2.0f);
				float va = (a1 > 0.1f) ? a1 : 1.0f;
				// Draw line from (x1,y1) to (x2,y2) with color
				vulkanGraphics->setColor(love::Colorf(vr, vg, vb, va));
				love::Vector2 lineVerts[2] = {
					love::Vector2(x1, y1),
					love::Vector2(x2, y2)
				};
				vulkanGraphics->polyline(lineVerts, 2);
			}

			// Restore the previous projection/transform
			vulkanGraphics->resetProjection();
			vulkanGraphics->pop();
		}
		
		// Clear debug geometry for next frame
		app->chai_collisions.clearDebugRendererGeometry(0);
	} catch (const std::exception& e) {
		printf("[DEBUG RENDERER ERROR] Exception in debug rendering: %s\n", e.what());
		fflush(stdout);
	} catch (...) {
		printf("[DEBUG RENDERER ERROR] Unknown exception in debug rendering\n");
		fflush(stdout);
	}
	runCount++;
	#endif // ENABLE_DEBUG_GEOMETRY_RENDERING
	#endif // JPH_DEBUG_RENDERER

	// Copy the video buffer to the screen.
	// video_cb(app->videoBuffer, app->config.window.width, app->config.window.height, app->config.window.width << 2);
	if (!app->event.m_pauserendering) {
			
		// PERF: Removed blocking wait_sync_index() - let GPU run asynchronously
		// vulkan->wait_sync_index(vulkan->handle);

		// auto cmd = cg.instance->getCommandBuffersForDataTransfer(); 
		// VkCommandBuffer cmd[buffers.size()];
		// for (size_t i = 0; i < buffers.size(); i++) {
		// 	cmd[i] = buffers[i];
		// }		

		// auto submitStart = std::chrono::high_resolution_clock::now();
		vulkanGraphics->submitGpuCommands(love::gfx::vulkan::SUBMIT_NOPRESENT, nullptr);
		// auto submitEnd = std::chrono::high_resolution_clock::now();
		vk.index = vulkan->get_sync_index(vulkan->handle);
		
		// NOTE: Debug rendering now uses app->graphics.line() - no pipeline creation needed
		
		// CRITICAL FIX: Advance frame after submitting GPU commands in libretro mode
		// This ensures each frame uses a different command buffer and prevents "every other frame" persistence
		// advanceFrame() internally calls beginFrame() to start recording the next frame's command buffer
		// std::printf("[FRAMEADVANCE] Advancing to next frame\n");
		// fflush(stdout);
		// auto advanceStart = std::chrono::high_resolution_clock::now();
		// vulkanGraphics->advanceFrame();
		// auto advanceEnd = std::chrono::high_resolution_clock::now();

		retro_vulkan_image image;
		image.image_view = cg.instance->getCurrentSwapchainImageView();
		image.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		image.create_info = cg.instance->getCurrentSwapchainImageViewCreateInfo();
		vulkan->set_image(vulkan->handle, &image, 0, NULL, VK_QUEUE_FAMILY_IGNORED);
		// CRITICAL FIX: Don't set command buffers here - already done in submitGpuCommands()
		// This was causing duplicate render passes with clearing, making Pass #2 wipe out Pass #1
   		// vulkan->set_command_buffers(vulkan->handle, 1, cmd);
		video_cb(RETRO_HW_FRAME_BUFFER_VALID, app->chai_gfx.width, app->chai_gfx.height, 0);
		
		// PERF: Print timing breakdown every 60 frames
		auto frameEnd = std::chrono::high_resolution_clock::now();
		if (++perfCounter >= 60) {
			perfCounter = 0;
			// auto updateMs = std::chrono::duration<double, std::milli>(updateEnd - updateStart).count();
			// auto drawMs = std::chrono::duration<double, std::milli>(drawEnd - drawStart).count();
			// auto submitMs = std::chrono::duration<double, std::milli>(submitEnd - submitStart).count();
			// auto advanceMs = std::chrono::duration<double, std::milli>(advanceEnd - advanceStart).count();
			// auto totalMs = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
			// std::printf("[PERF] Frame: %.2f ms | Update: %.2f ms | Draw: %.2f ms | Submit: %.2f ms | Advance: %.2f ms\n",
			// 	totalMs, updateMs, drawMs, submitMs, advanceMs);
			// fflush(stdout);
		}
	}

	// CRITICAL FIX: Process cleanup callbacks EVERY frame to release staging buffers immediately
	// Staging buffers, StreamBuffers, and other resources queue cleanup via queueCleanUp()
	// In normal mode, beginFrame() processes these every frame - libretro mode must do the same
	vulkanGraphics->processCleanupCallbacks();
	
	// PERF: Throttle shader resets and staging buffer cleanup to reduce per-frame overhead
	static int shaderResetCounter = 0;
	if (++shaderResetCounter >= 60)
	{
		shaderResetCounter = 0;
		// Call shader newFrame() to prevent descriptor pool/pipeline memory leaks
		// The shader newFrame() has built-in throttling (pools every 10 frames, pipelines every 60)
		vulkanGraphics->callShaderNewFrame();
		
		// CRITICAL FIX: Clean up unused staging buffers to prevent pool accumulation
		// Staging buffers are released via callbacks but remain in pool - remove unused ones
		vulkanGraphics->cleanupUnusedStagingBuffers();
	}

	// One-time aggressive cleanup after first frame to release initialization memory
	static bool firstFrameCleanupDone = false;
	if (!firstFrameCleanupDone && ++statsFrameCounter > 1) {
		firstFrameCleanupDone = true;
		
		HANDLE hProcess = GetCurrentProcess();
		SetProcessWorkingSetSize(hProcess, (SIZE_T)-1, (SIZE_T)-1);
		EmptyWorkingSet(hProcess);
		_heapmin();
	}

	// See if the game requested to close itself.
	if (app->event.m_shouldclose) {
		ChaiLove::environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, 0);
	}
}

#ifdef _3DS
extern "C" int nanosleep(const struct timespec *req, struct timespec *rem) {
  return 0;
}
#endif

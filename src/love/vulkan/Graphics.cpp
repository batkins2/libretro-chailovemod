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

#define VOLK_IMPLEMENTATION
#define VMA_IMPLEMENTATION

#include "../common/Exception.h"
#include "../common/pixelformat.h"
#include "../common/version.h"
#include "../common/memory.h"
#include "../window/Window.h"
#include "Buffer.h"
#include "Graphics.h"
#include "GraphicsReadback.h"
#include "Shader.h"
#include "Vulkan.h"

#include <SDL2/SDL_vulkan.h>

#include <algorithm>
#include <vector>
#include <cstring>
#include <set>
#include <sstream>
#include <array>

#include "../../ChaiLove.h"


// #include "vma/vk_mem_alloc.h"

namespace love
{
namespace gfx
{
namespace vulkan
{

static const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

static const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

constexpr int DEFAULT_VERTEX_BUFFER_BINDING = 0;
constexpr int VERTEX_BUFFER_BINDING_START = 1;

VkDevice Graphics::getDevice() const
{
	return device;
}

VmaAllocator Graphics::getVmaAllocator() const
{
	return vmaAllocator;
}

VkImageView Graphics::getCurrentSwapchainImageView()
{
    // In libretro mode, use fakeBackbuffer instead of swapchain
    if (libretroMode && fakeBackbuffer != nullptr) {
        return fakeBackbuffer->getRenderTargetView(0, 0);
    }
    
    if (imageIndex >= swapChainImageViews.size())
        return VK_NULL_HANDLE;
    return swapChainImageViews[imageIndex];
}

VkImageViewCreateInfo Graphics::getCurrentSwapchainImageViewCreateInfo()
{
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    
    // In libretro mode, use fakeBackbuffer instead of swapchain
    if (libretroMode && fakeBackbuffer != nullptr) {
        createInfo.image = (VkImage)fakeBackbuffer->getRenderTargetHandle();
        createInfo.format = Vulkan::getTextureFormat(fakeBackbuffer->getPixelFormat()).internalFormat;
    } else if (swapChainImageViews.empty() || imageIndex >= swapChainImageViews.size()) {
        createInfo.image = VK_NULL_HANDLE;
        createInfo.format = swapChainImageFormat;
    } else {
        createInfo.image = swapChainImages[imageIndex];
        createInfo.format = swapChainImageFormat;
    }
    
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    return createInfo;
}

static void checkOptionalInstanceExtensions(OptionalInstanceExtensions& ext)
{
	uint32_t count;

	vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

	std::vector<VkExtensionProperties> extensions(count);

	vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());

	for (const auto& extension : extensions)
	{
		if (strcmp(extension.extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0)
			ext.physicalDeviceProperties2 = true;
		if (strcmp(extension.extensionName, VK_EXT_DEBUG_MARKER_EXTENSION_NAME) == 0)
			ext.debugInfo = true;
	}
}

Graphics::Graphics()
    : love::gfx::Graphics("love.graphics.vulkan")
{
    // std::printf("[CHAILOVE DEBUG] Graphics::Graphics() constructor called\n");
    
	// Initialize magic number first for corruption detection
    magicNumber = GRAPHICS_MAGIC;

	commandBufferRecording = false;  // Initialize command buffer recording flag

    if (SDL_Vulkan_LoadLibrary(nullptr) < 0)
        throw love::Exception("Could not load Vulkan library.");

    volkInitializeCustom((PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr());

    if (isDebugEnabled() && !checkValidationSupport())
        throw love::Exception("Validation layers requested, but not available!");

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "LOVE";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);	// get this version from somewhere else?
    appInfo.pEngineName = "LOVE Game Framework";
    appInfo.engineVersion = VK_MAKE_VERSION(0, VERSION_MAJOR, VERSION_MINOR);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.pNext = nullptr;
   
    // GetInstanceExtensions works with a null window parameter as long as
    // SDL_Vulkan_LoadLibrary has been called (which we do earlier).
    unsigned int count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &count, nullptr)) {
        throw love::Exception("Failed to get SDL Vulkan instance extensions count.");
    }

    std::vector<const char*> extensions(count);
    if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &count, extensions.data())) {
        throw love::Exception("Failed to get SDL Vulkan instance extensions.");
    }

    checkOptionalInstanceExtensions(optionalInstanceExtensions);

    if (optionalInstanceExtensions.physicalDeviceProperties2)
        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    if (optionalInstanceExtensions.debugInfo)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (isDebugEnabled())
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        throw love::Exception("Failed to create Vulkan instance.");

    volkLoadInstance(instance);

    // std::printf("[CHAILOVE DEBUG] Graphics::Graphics() constructor completed\n");
}

Graphics::~Graphics()
{
	// Immediately mark magic number as destroyed to detect use-after-free
    magicNumber = 0xDEADBEEF;

    // Immediately mark as destroyed to prevent further access
    created = false;
    if (magicNumber == GRAPHICS_MAGIC) {  // Only clear if it was valid
        magicNumber = 0xDEADBEEF;
    }
    
    defaultVertexBuffer.set(nullptr);
    localUniformBuffer.set(nullptr);

    Volatile::unloadAll();
    cleanup();
    
    // Clean up command pool if we created it
    if (ownsCommandPool && commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
    }
    
    vkDestroyInstance(instance, nullptr);

    SDL_Vulkan_UnloadLibrary();
}

bool Graphics::bindVAO()
{
    // Vulkan doesn't use VAOs like OpenGL - vertex input is handled via pipelines
    // This is a compatibility function for the base graphics interface
    return true; // Return true to indicate success
}

// START OVERRIDEN FUNCTIONS

love::gfx::Texture *Graphics::newTexture(const love::gfx::Texture::Settings &settings, const love::gfx::Texture::Slices *data)
{
	return new Texture(this, settings, data);
}

love::gfx::Texture *Graphics::newTextureView(love::gfx::Texture *base, const Texture::ViewSettings &viewsettings)
{
	return new Texture(this, base, viewsettings);
}

love::gfx::Buffer *Graphics::newBuffer(const love::gfx::Buffer::Settings &settings, const std::vector<love::gfx::Buffer::DataDeclaration> &format, const void *data, size_t size, size_t arraylength)
{
	return new Buffer(this, settings, format, data, size, arraylength);
}

love::gfx::Buffer *Graphics::newBuffer(const love::gfx::Buffer::Settings &settings, love::gfx::DataFormat format, const void *data, size_t size, size_t arraylength)
{
    // Convert single DataFormat to vector of DataDeclarations
    std::vector<love::gfx::Buffer::DataDeclaration> formatVector;
    
    // Create a single declaration with the provided format
    formatVector.emplace_back("data", format);
    
    // Call the existing implementation
    return newBuffer(settings, formatVector, data, size, arraylength);
}

void Graphics::clear(OptionalColorD color, OptionalInt stencil, OptionalDouble depth)
{
	if (!color.hasValue && !stencil.hasValue && !depth.hasValue)
		return;

	std::vector<OptionalColorD> colors;

	if (color.hasValue)
		colors.resize(std::max(1, (int)states.back().renderTargets.colors.size()), color);

	clear(colors, stencil, depth);
}

void Graphics::clear(const std::vector<OptionalColorD> &colors, OptionalInt stencil, OptionalDouble depth)
{
	if (colors.empty() && !stencil.hasValue && !depth.hasValue)
		return;

	flushBatchedDraws();

	const auto &rts = states.back().renderTargets;
	bool rtactive = isRenderTargetActive();
	size_t ncolorbuffers = rtactive ? rts.colors.size() : 1;
	size_t ncolors = std::min(ncolorbuffers, colors.size());

	if (renderPassState.active)
	{
		std::vector<VkClearAttachment> attachments;
		for (size_t i = 0; i < ncolors; i++)
		{
			const OptionalColorD &color = colors[i];
			VkClearAttachment attachment{};
			if (color.hasValue)
			{
				auto texture = i < rts.colors.size() ? rts.colors[i].texture.get() : nullptr;
				attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				attachment.clearValue.color = Texture::getClearColor(texture, color.value);
			}
			attachments.push_back(attachment);
		}

		VkClearAttachment depthStencilAttachment{};

		auto dstexture = rts.depthStencil.texture.get();

		if (stencil.hasValue)
		{
			if ((!rtactive && backbufferHasStencil)
				|| (dstexture && isPixelFormatStencil(dstexture->getPixelFormat())) || (rts.temporaryRTFlags & TEMPORARY_RT_STENCIL) != 0)
			{
				depthStencilAttachment.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
				depthStencilAttachment.clearValue.depthStencil.stencil = static_cast<uint32_t>(stencil.value);
			}
		}
		if (depth.hasValue)
		{
			if ((!rtactive && backbufferHasDepth)
				|| (dstexture && isPixelFormatDepth(dstexture->getPixelFormat())) || (rts.temporaryRTFlags & TEMPORARY_RT_DEPTH) != 0)
			{
				depthStencilAttachment.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
				depthStencilAttachment.clearValue.depthStencil.depth = static_cast<float>(depth.value);
			}
		}

		if (depthStencilAttachment.aspectMask != 0)
			attachments.push_back(depthStencilAttachment);

		VkClearRect rect{};
		rect.layerCount = 1;
		rect.rect.extent.width = static_cast<uint32_t>(renderPassState.width);
		rect.rect.extent.height = static_cast<uint32_t>(renderPassState.height);

		vkCmdClearAttachments(
			commandBuffers[currentFrame],
			static_cast<uint32_t>(attachments.size()), attachments.data(),
			1, &rect);
	}
	else
	{
		for (size_t i = 0; i < ncolors; i++)
		{
			if (colors[i].hasValue)
			{
				renderPassState.renderPassConfiguration.colorAttachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

				auto texture = i < rts.colors.size() ? rts.colors[i].texture.get() : nullptr;
				renderPassState.clearColors[i].color = Texture::getClearColor(texture, colors[i].value);
			}
		}

		if (depth.hasValue)
		{
			renderPassState.renderPassConfiguration.staticData.depthStencilAttachment.depthLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			renderPassState.clearColors[ncolorbuffers].depthStencil.depth = static_cast<float>(depth.value);
		}

		if (stencil.hasValue)
		{
			renderPassState.renderPassConfiguration.staticData.depthStencilAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			renderPassState.clearColors[ncolorbuffers].depthStencil.stencil = static_cast<uint32_t>(stencil.value);
		}

		if (renderPassState.isWindow)
		{
			renderPassState.windowClearRequested = true;
			renderPassState.mainWindowClearColorValue = colors.empty() ? OptionalColorD() : colors[0];
			renderPassState.mainWindowClearDepthValue = depth;
			renderPassState.mainWindowClearStencilValue = stencil;
		}
		// else
		// 	startRenderPass();
	}
}

void Graphics::discard(const std::vector<bool> &colorbuffers, bool depthstencil)
{
	if (renderPassState.active)
		endRenderPass();

	auto &renderPassConfiguration = renderPassState.renderPassConfiguration;

	for (size_t i = 0; i < colorbuffers.size(); i++)
	{
		if (colorbuffers[i])
			renderPassConfiguration.colorAttachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	}

	if (depthstencil)
	{
		renderPassConfiguration.staticData.depthStencilAttachment.depthLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassConfiguration.staticData.depthStencilAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	}

	startRenderPass();
}

void Graphics::submitGpuCommands(SubmitMode submitMode, void *screenshotCallbackData)
{
    // std::printf("[CHAILOVE DEBUG] submitGpuCommands called:\n");
    // std::printf("[CHAILOVE DEBUG] - libretroMode: %s\n", libretroMode ? "true" : "false");
    // std::printf("[CHAILOVE DEBUG] - submitMode: %d (SUBMIT_NOPRESENT=%d, SUBMIT_PRESENT=%d)\n", 
        //    submitMode, SUBMIT_NOPRESENT, SUBMIT_PRESENT);
    // std::printf("[CHAILOVE DEBUG] - renderPassState.active: %s\n", renderPassState.active ? "true" : "false");
    
    if (graphicsQueue == VK_NULL_HANDLE) {
        // std::printf("[CHAILOVE DEBUG] Graphics queue is null, cannot submit\n");
        return;
    }
    
    if (device == VK_NULL_HANDLE) {
        // std::printf("[CHAILOVE DEBUG] Device is null, cannot submit\n");
        return;
    }

    // LIBRETRO FIX: Force startRenderPass() in libretro mode when submitting with SUBMIT_NOPRESENT
    if (libretroMode && submitMode == SUBMIT_NOPRESENT && !renderPassState.active) {
        // std::printf("[CHAILOVE DEBUG] LIBRETRO MODE: Forcing startRenderPass() before submitGpuCommands(SUBMIT_NOPRESENT)\n");
        
        // Force windowClearRequested if not already set
        if (!renderPassState.windowClearRequested) {
            // std::printf("[CHAILOVE DEBUG] Setting windowClearRequested = true for libretro\n");
            renderPassState.windowClearRequested = true;
            
            // Set a default clear color if none is set - use BRIGHT GREEN to be sure
            renderPassState.mainWindowClearColorValue.hasValue = true;
            renderPassState.mainWindowClearColorValue.value = ColorD(0.0, 1.0, 0.0, 1.0); // Bright green 
            // std::printf("[CHAILOVE DEBUG] Set default clear color to bright GREEN\n");
        }
        
        // startRenderPass();
        // std::printf("[CHAILOVE DEBUG] startRenderPass() called successfully before submit\n");
    } else {
        // std::printf("[CHAILOVE DEBUG] NOT triggering libretro fix because:\n");
        // std::printf("[CHAILOVE DEBUG] - libretroMode: %s\n", libretroMode ? "true" : "false");
        // std::printf("[CHAILOVE DEBUG] - submitMode == SUBMIT_NOPRESENT: %s\n", (submitMode == SUBMIT_NOPRESENT) ? "true" : "false");
        // std::printf("[CHAILOVE DEBUG] - !renderPassState.active: %s\n", !renderPassState.active ? "true" : "false");
    }

    flushBatchedDraws();
    
    // ...rest of function...

    if (renderPassState.active)
        endRenderPass();

    VkBuffer screenshotBuffer = VK_NULL_HANDLE;
    VmaAllocation screenshotAllocation = VK_NULL_HANDLE;
    VmaAllocationInfo screenshotAllocationInfo = {};

    endRecordingGraphicsCommands();

    // In libretro mode, we still need to submit the commands
    if (libretroMode) {
        // std::printf("[CHAILOVE DEBUG] Libretro mode: submitting commands directly\n");
        
        // Get the current Vulkan interface
        auto vulkan = ChaiLove::getInstance()->chai_gfx.vulkan;
        if (!vulkan) {
            throw love::Exception("Vulkan interface not available in libretro mode");
        }
        
        // Wait for the current index to be available
        vulkan->wait_sync_index(vulkan->handle);
        
        // Get the sync index
        uint32_t sync_index = vulkan->get_sync_index(vulkan->handle);
        // std::printf("[CHAILOVE DEBUG] Got sync index: %u\n", sync_index);
        
        // Create array of command buffers for this frame
        std::array<VkCommandBuffer, 1> libretroCommandBuffers = { commandBuffers.at(currentFrame) };
        
        // Set the command buffers for RetroArch
        vulkan->set_command_buffers(vulkan->handle, static_cast<unsigned>(libretroCommandBuffers.size()), libretroCommandBuffers.data());
        
        // std::printf("[CHAILOVE DEBUG] Set command buffers for RetroArch\n");
        
        // Note: We don't submit here - RetroArch will handle the submission
        // This is different from the normal present() flow
        
        if (submitMode == SUBMIT_NOPRESENT || submitMode == SUBMIT_RESTART || screenshotBuffer != VK_NULL_HANDLE)
        {
            // Handle screenshot readback if needed
        }
        
        // std::printf("[CHAILOVE DEBUG] Libretro command submission completed\n");
        return;
    }

    // Instead of managing our own semaphores, work with RetroArch's system
    std::array<VkCommandBuffer, 1> submitCommandbuffers = { commandBuffers.at(currentFrame) };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    
    // Let RetroArch handle the semaphores - don't use our own
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = nullptr;

    submitInfo.commandBufferCount = static_cast<uint32_t>(submitCommandbuffers.size());
    submitInfo.pCommandBuffers = submitCommandbuffers.data();

    // Don't signal our own semaphores - RetroArch will handle this
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;

    auto vulkan = ChaiLove::getInstance()->chai_gfx.vulkan;
    if (vulkan->lock_queue) {
        vulkan->lock_queue(vulkan->handle);
    }

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
        throw love::Exception("failed to submit draw command buffer!");

    if (vulkan->unlock_queue) {
        vulkan->unlock_queue(vulkan->handle);
    }

    if (submitMode == SUBMIT_NOPRESENT || submitMode == SUBMIT_RESTART || screenshotBuffer != VK_NULL_HANDLE)
    {
        // if (screenshotBuffer != VK_NULL_HANDLE)
        // {
        //     auto readbackMemory = vmaAllocator.mapMemory(screenshotAllocation);

        //     readbackCallbacks.at(currentFrame).push_back([this, readbackMemory, screenshotAllocationInfo, screenshotCallbackData, screenshotBuffer, screenshotAllocation]()
        //     {
        //         if (screenshotCallbackData != nullptr)
        //         {
        //             void(*callback)(void *data, size_t size, void *userdata) = (void(*)(void*, size_t, void*)) screenshotCallbackData;
        //             callback(readbackMemory, screenshotAllocationInfo.size, nullptr);
        //         }

        //         vmaUnmapMemory(vmaAllocator, screenshotAllocation);
        //         vmaDestroyBuffer(vmaAllocator, screenshotBuffer, screenshotAllocation);
        //     });
        // }

        if (submitMode != SUBMIT_RESTART)
        {
            // vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
            // vkResetFences(device, 1, &inFlightFences[currentFrame]);

            updatePendingReadbacks();
            updateTemporaryResources();

            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

            beginFrame();
        }
    }
}

void Graphics::present(void *screenshotCallbackdata)
{
    if (!isActive())
        throw love::Exception("present can only be called while the graphics module is active.");

    if (isRenderTargetActive())
        throw love::Exception("present cannot be called while a render target is active.");

    // LIBRETRO FIX: Force startRenderPass() in libretro mode to trigger clearing
    if (libretroMode && !renderPassState.active) {
        // std::printf("[CHAILOVE DEBUG] LIBRETRO MODE: Forcing startRenderPass() to trigger clearing\n");
        
        // Force windowClearRequested if not already set
        // if (!renderPassState.windowClearRequested) {
        //     // std::printf("[CHAILOVE DEBUG] Setting windowClearRequested = true for libretro\n");
        //     renderPassState.windowClearRequested = true;
            
        //     // Set a default clear color if none is set
        //     renderPassState.mainWindowClearColorValue.hasValue = true;
        //     renderPassState.mainWindowClearColorValue.value = ColorD(1.0, 0.0, 1.0, 1.0); // Bright magenta for visibility
        //     // std::printf("[CHAILOVE DEBUG] Set default clear color to bright magenta\n");
        // }
        
        startRenderPass();
        // std::printf("[CHAILOVE DEBUG] startRenderPass() called successfully\n");
    } else if (!renderPassState.active && renderPassState.windowClearRequested) {
        // Original logic for non-libretro mode
        startRenderPass();
    }

    deprecations.draw(this);

    submitGpuCommands(SUBMIT_PRESENT, screenshotCallbackdata);

    VkResult result = VK_SUCCESS;

        // In libretro mode, skip all swapchain/surface operations
    if (!libretroMode) {
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || swapChainRecreationRequested)
        {
            swapChainRecreationRequested = false;
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS)
            throw love::Exception("failed to present swap chain image!");
    }

    // Process the 2 vertex buffers in batchedDrawState
    // std::printf("[CHAILOVE DEBUG] Processing batchedDrawState buffers\n");
    
    for (int i = 0; i < 2; i++) {
        if (batchedDrawState.vb[i] != nullptr) {
            // std::printf("[CHAILOVE DEBUG] - vb[%d]: %p\n", i, batchedDrawState.vb[i]);
            if (batchedDrawState.vb[i]->getSize() > 0) {
                // std::printf("[CHAILOVE DEBUG]   Size: %zu bytes\n", batchedDrawState.vb[i]->getSize());
            }
        } else {
            // std::printf("[CHAILOVE DEBUG] - vb[%d]: nullptr\n", i);
        }
    }
    
    if (batchedDrawState.indexBuffer != nullptr) {
        // std::printf("[CHAILOVE DEBUG] - indexBuffer: %p\n", batchedDrawState.indexBuffer);
        if (batchedDrawState.indexBuffer->getSize() > 0) {
            // std::printf("[CHAILOVE DEBUG]   Size: %zu bytes\n", batchedDrawState.indexBuffer->getSize());
        }
    } else {
        // std::printf("[CHAILOVE DEBUG] - indexBuffer: nullptr\n");
    }

    drawCalls = 0;
    renderTargetSwitchCount = 0;
    drawCallsBatched = 0;

    updatePendingReadbacks();
    updateTemporaryResources();

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    beginFrame();
}

void Graphics::backbufferChanged(int width, int height, int pixelwidth, int pixelheight, bool backbufferstencil, bool backbufferdepth, int msaa)
{
	if (swapChain != VK_NULL_HANDLE && (pixelwidth != this->pixelWidth || pixelheight != this->pixelHeight || width != this->width || height != this->height
		|| backbufferstencil != this->backbufferHasStencil || backbufferdepth != this->backbufferHasDepth || msaa != requestedMsaa))
		requestSwapchainRecreation();

	this->width = width;
	this->height = height;
	this->pixelWidth = pixelwidth;
	this->pixelHeight = pixelheight;

	this->backbufferHasStencil = backbufferstencil;
	this->backbufferHasDepth = backbufferdepth;
	this->requestedMsaa = msaa;

	if (!isRenderTargetActive())
		resetProjection();

	if (swapChain != VK_NULL_HANDLE)
		msaaSamples = getMsaaCount(requestedMsaa);
}

bool Graphics::setMode(void *context, int width, int height, int pixelwidth, int pixelheight, bool backbufferstencil, bool backbufferdepth, int msaa)
{
    // std::printf("[CHAILOVE DEBUG] setMode called, libretroMode = %s, externalInstance = %p\n", 
        //    libretroMode ? "true" : "false", externalInstance);
           
    if (libretroMode && externalInstance != VK_NULL_HANDLE) {
        // std::printf("[CHAILOVE DEBUG] Entering libretro initialization path\n");
        
        // Use RetroArch's Vulkan context instead of creating our own
        instance = externalInstance;
        device = externalDevice;
        physicalDevice = externalPhysicalDevice;
        graphicsQueue = externalQueue;
		
        // Only set command pool if we don't already have one
        if (commandPool == VK_NULL_HANDLE) {
            commandPool = externalCommandPool;
        }
        
        if (vmaAllocator == VK_NULL_HANDLE) {
            VmaAllocatorCreateInfo allocatorCreateInfo = {};
            allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_0;
            allocatorCreateInfo.physicalDevice = physicalDevice;
            allocatorCreateInfo.device = device;
            allocatorCreateInfo.instance = instance;
            
            vmaVulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
            vmaVulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
            vmaVulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
            vmaVulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
            vmaVulkanFunctions.vkAllocateMemory = vkAllocateMemory;
            vmaVulkanFunctions.vkFreeMemory = vkFreeMemory;
            vmaVulkanFunctions.vkMapMemory = vkMapMemory;
            vmaVulkanFunctions.vkUnmapMemory = vkUnmapMemory;
            vmaVulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
            vmaVulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
            vmaVulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
            vmaVulkanFunctions.vkBindImageMemory = vkBindImageMemory;
            vmaVulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
            vmaVulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
            vmaVulkanFunctions.vkCreateBuffer = vkCreateBuffer;
            vmaVulkanFunctions.vkCreateImage = vkCreateImage;
            vmaVulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
            vmaVulkanFunctions.vkDestroyImage = vkDestroyImage;
            vmaVulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;

            vmaVulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
            vmaVulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR;
            vmaVulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2KHR;
            vmaVulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2KHR;
            vmaVulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR;
            vmaVulkanFunctions.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;
            vmaVulkanFunctions.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;

            allocatorCreateInfo.pVulkanFunctions = &vmaVulkanFunctions;
            allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;

            if (vmaCreateAllocator(&allocatorCreateInfo, &vmaAllocator) != VK_SUCCESS)
                throw love::Exception("Failed to create VMA allocator in libretro mode");
        }
        
        // Set up the rest of the graphics state for libretro
        backbufferChanged(width, height, pixelwidth, pixelheight, backbufferstencil, backbufferdepth, msaa);

        // std::printf("[CHAILOVE DEBUG] About to call initCapabilities() in libretro mode\n");
        // Initialize capabilities with the external device - MUST BE DONE EARLY
        initCapabilities();
        // std::printf("[CHAILOVE DEBUG] initCapabilities() completed in libretro mode\n");

        // Initialize cleanup and callback vectors
        cleanUpFunctions.clear();
        cleanUpFunctions.resize(MAX_FRAMES_IN_FLIGHT);
        readbackCallbacks.clear();
        readbackCallbacks.resize(MAX_FRAMES_IN_FLIGHT);
        
        // Create command buffers from RetroArch's command pool
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;  // Use RetroArch's command pool
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
            throw love::Exception("Failed to allocate command buffers in libretro mode");
        
        // Create our internal resources that don't conflict with RetroArch
        if (localUniformBuffer == nullptr)
            localUniformBuffer.set(new StreamBuffer(this, BUFFERUSAGE_UNIFORM, 1024 * 512 * 1), Acquire::NORETAIN);
            
        // Create defaultVertexBuffer
        if (defaultVertexBuffer == nullptr) {
            std::vector<Buffer::DataDeclaration> defaultVertexFormat = {
                {"DefaultFloat", DATAFORMAT_FLOAT_VEC4},
                {"DefaultInt", DATAFORMAT_INT32_VEC4},
                {"DefaultColor", DATAFORMAT_FLOAT_VEC4},
            };
            
            Buffer::Settings settings(BUFFERUSAGEFLAG_VERTEX, BUFFERDATAUSAGE_STATIC);
            
            defaultVertexBuffer.set(new Buffer(this, settings, defaultVertexFormat, nullptr, sizeof(float) * 8 * 4, 4), Acquire::NORETAIN);
        }

        // Set MSAA
        msaaSamples = getMsaaCount(requestedMsaa);
        
        // Create color resources (for MSAA if needed)
        createColorResources();
        
        // Create depth resources if needed
        // If we have a physical device but depthStencilFormat is undefined, reinitialize it
        if (backbufferstencil || backbufferdepth) {
            // If we have a physical device but depthStencilFormat is undefined, initialize it
            if (depthStencilFormat == VK_FORMAT_UNDEFINED) {
                if (physicalDevice != VK_NULL_HANDLE) {
                    // Reinitialize depth format - this can happen during reinitialization
                    // std::printf("[CHAILOVE DEBUG] Reinitializing depthStencilFormat during setMode\n");
                    depthStencilFormat = findDepthFormat();
                    switch (depthStencilFormat)
                    {
                    case VK_FORMAT_D32_SFLOAT:
                        depthStencilPixelFormat = PIXELFORMAT_DEPTH32_FLOAT;
                        break;
                    case VK_FORMAT_D32_SFLOAT_S8_UINT:
                        depthStencilPixelFormat = PIXELFORMAT_DEPTH32_FLOAT_STENCIL8;
                        break;
                    case VK_FORMAT_D24_UNORM_S8_UINT:
                        depthStencilPixelFormat = PIXELFORMAT_DEPTH24_UNORM_STENCIL8;
                        break;
                    default:
                        throw love::Exception("Failed to convert vulkan depth/stencil swapchain pixel format %d to love PixelFormat.", depthStencilFormat);
                        break;
                    }
                } else {
                    // std::printf("[CHAILOVE DEBUG] Skipping depth resource creation - no physical device\n");
                }
            }
            
            // Only create depth resources if we have a valid format
            if (depthStencilFormat != VK_FORMAT_UNDEFINED) {
                // std::printf("[CHAILOVE DEBUG] Creating depth resources with format %d\n", depthStencilFormat);
                createDepthResources();
            } else {
                // std::printf("[CHAILOVE DEBUG] Skipping depth resource creation - undefined format\n");
            }
        }
        
        transitionColorDepthLayouts = true;

        // Create our render target texture for libretro
        auto settings = love::gfx::Texture::Settings{
                    width, height, 1,
                    love::gfx::TEXTURE_2D,
                    love::gfx::Texture::MipmapsMode::MIPMAPS_NONE,
                    0,
                    love::PixelFormat::PIXELFORMAT_NORMAL,
                    false,
                    1.0f,
                    1,
                    true  // isRenderTarget = true
                };
        fakeBackbuffer.set(new Texture(this, settings, nullptr), Acquire::NORETAIN);

        // Create default shaders
        createDefaultShaders();

        // Create quad index buffer (needed for drawQuads function)
        if (quadIndexBuffer == nullptr) {
            std::vector<uint16> quadIndices;
            quadIndices.reserve(LOVE_UINT16_MAX);

            for (uint16 i = 0; i < LOVE_UINT16_MAX - 3; i += 4)
            {
                quadIndices.push_back(i + 0);
                quadIndices.push_back(i + 1);
                quadIndices.push_back(i + 2);

                quadIndices.push_back(i + 0);
                quadIndices.push_back(i + 2);
                quadIndices.push_back(i + 3);
            }

            size_t quadIndexDataSize = quadIndices.size() * sizeof(uint16);

            Buffer::Settings quadIndexBufferSettings(BUFFERUSAGEFLAG_INDEX, BUFFERDATAUSAGE_STATIC);

            std::vector<Buffer::DataDeclaration> quadIndexFormat = {
                {"index", DATAFORMAT_UINT16},
            };

            quadIndexBuffer = new Buffer(this, quadIndexBufferSettings, quadIndexFormat, quadIndices.data(), quadIndexDataSize, quadIndices.size());
        }

        // Begin frame to initialize command buffer state
        beginFrame();

        // Final setup
        restoreState(states.back());
        Vulkan::resetShaderSwitches();
        created = true;
        drawCalls = 0;
        drawCallsBatched = 0;

        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 }
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]);
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = 100;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }

        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 1;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboLayoutBinding;

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }

        // std::printf("[CHAILOVE DEBUG] Libretro initialization completed successfully\n");
        // RETURN EARLY - don't continue with normal initialization
        return true;
    }

    // std::printf("[CHAILOVE DEBUG] Taking normal (non-libretro) initialization path\n");
    // Normal (non-libretro) initialization path starts here...
    // Must be called before the swapchain is created.
    backbufferChanged(width, height, pixelwidth, pixelheight, backbufferstencil, backbufferdepth, msaa);

    cleanUpFunctions.clear();
    cleanUpFunctions.resize(MAX_FRAMES_IN_FLIGHT);

    readbackCallbacks.clear();
    readbackCallbacks.resize(MAX_FRAMES_IN_FLIGHT);

    bool createBaseObjects = physicalDevice == VK_NULL_HANDLE;

    createSurface();

    if (createBaseObjects)
    {
        pickPhysicalDevice();
        createLogicalDevice();
        createPipelineCache();
        initVMA();
        initCapabilities();
    }

    msaaSamples = getMsaaCount(requestedMsaa);

    // For libretro mode, skip swapchain creation but create our own render targets
    // createSwapChain();  // Skip this for libretro
    // createImageViews();
    createColorResources();
    
    // Only create depth resources if we actually need them AND they haven't been created yet
    if ((backbufferstencil || backbufferdepth) && depthImageView == VK_NULL_HANDLE) {
        // std::printf("[CHAILOVE DEBUG] Creating depth resources in second initialization path\n");
        createDepthResources();
    }
    
    transitionColorDepthLayouts = true;

    if (createBaseObjects)
    {
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
    }

    if (localUniformBuffer == nullptr)
        localUniformBuffer.set(new StreamBuffer(this, BUFFERUSAGE_UNIFORM, 1024 * 512 * 1), Acquire::NORETAIN);

    // Create our render target texture for libretro
    auto settings = love::gfx::Texture::Settings{
                width, height, 1,
                love::gfx::TEXTURE_2D,
                love::gfx::Texture::MipmapsMode::MIPMAPS_NONE,
                0,
                love::PixelFormat::PIXELFORMAT_NORMAL,
                false,
                1.0f,
                1,
                true  // isRenderTarget = true
            };
    fakeBackbuffer.set(new Texture(this, settings, nullptr), Acquire::NORETAIN);

    if (createBaseObjects)
    {
        // Create default shaders
        createDefaultShaders();

        // Create quad index buffer (needed for drawQuads function)
        if (quadIndexBuffer == nullptr) {
            std::vector<uint16> quadIndices;
            quadIndices.reserve(LOVE_UINT16_MAX);

            for (uint16 i = 0; i < LOVE_UINT16_MAX - 3; i += 4)
            {
                quadIndices.push_back(i + 0);
                quadIndices.push_back(i + 1);
                quadIndices.push_back(i + 2);

                quadIndices.push_back(i + 0);
                quadIndices.push_back(i + 2);
                quadIndices.push_back(i + 3);
            }

            size_t quadIndexDataSize = quadIndices.size() * sizeof(uint16);

            Buffer::Settings quadIndexBufferSettings(BUFFERUSAGEFLAG_INDEX, BUFFERDATAUSAGE_STATIC);

            std::vector<Buffer::DataDeclaration> quadIndexFormat = {
                {"index", DATAFORMAT_UINT16},
            };

            quadIndexBuffer = new Buffer(this, quadIndexBufferSettings, quadIndexFormat, quadIndices.data(), quadIndexDataSize, quadIndices.size());
        }
    }

    // Begin frame to initialize command buffer state
    beginFrame();

    restoreState(states.back());

    Vulkan::resetShaderSwitches();

    // Initialize BatchedDrawState buffers (similar to OpenGL implementation)
    if (batchedDrawState.vb[0] == nullptr) {
        // std::printf("[CHAILOVE DEBUG] Initializing BatchedDrawState buffers\n");
        batchedDrawState.vb[0] = newStreamBuffer(BUFFERUSAGE_VERTEX, 1024 * 1024 * 1);
        batchedDrawState.vb[1] = newStreamBuffer(BUFFERUSAGE_VERTEX, 256 * 1024 * 1);
        batchedDrawState.indexBuffer = newStreamBuffer(BUFFERUSAGE_INDEX, sizeof(uint16) * LOVE_UINT16_MAX);
    }

    created = true;
    drawCalls = 0;
    drawCallsBatched = 0;

    return true;
}

bool Graphics::setLibretroVulkanContext(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, 
                                        VkQueue queue, VkCommandPool commandPool)
{
    // std::printf("[CHAILOVE DEBUG] setLibretroVulkanContext called\n");
    // std::printf("[CHAILOVE DEBUG] - instance: %p\n", instance);
    // std::printf("[CHAILOVE DEBUG] - device: %p\n", device);
    // std::printf("[CHAILOVE DEBUG] - physicalDevice: %p\n", physicalDevice);
    // std::printf("[CHAILOVE DEBUG] - queue: %p\n", queue);
    // std::printf("[CHAILOVE DEBUG] - commandPool: %p\n", commandPool);
    
    libretroMode = true;
	commandBufferRecording = false;  // Initialize command buffer recording flag
    externalInstance = instance;
    externalDevice = device;
    externalPhysicalDevice = physicalDevice;
    externalQueue = queue;
    externalCommandPool = commandPool;
    
    // Set up the Vulkan objects so initCapabilities() can use them
    this->instance = externalInstance;
    this->device = externalDevice;
    this->physicalDevice = externalPhysicalDevice;
    this->graphicsQueue = externalQueue;
    
    // Create our own command pool if RetroArch doesn't provide one
    if (commandPool == VK_NULL_HANDLE) {
        // std::printf("[CHAILOVE DEBUG] Creating our own command pool\n");
        // Find queue family for graphics queue
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
        
        uint32_t graphicsFamily = 0;
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphicsFamily = i;
                break;
            }
        }
        
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsFamily;
        
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &this->commandPool) != VK_SUCCESS) {
            throw love::Exception("Failed to create command pool in libretro mode");
        }
        
        ownsCommandPool = true;  // We created it, so we own it
        // std::printf("[CHAILOVE DEBUG] Created command pool: %p\n", this->commandPool);
    } else {
        this->commandPool = externalCommandPool;
        ownsCommandPool = false;  // RetroArch provided it
    }
    
    // Initialize VMA allocator if not already created
    if (vmaAllocator == VK_NULL_HANDLE) {
        // std::printf("[CHAILOVE DEBUG] Initializing VMA allocator\n");
        VmaAllocatorCreateInfo allocatorCreateInfo = {};
        allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_0;
        allocatorCreateInfo.physicalDevice = physicalDevice;
        allocatorCreateInfo.device = device;
        allocatorCreateInfo.instance = instance;
        
        vmaVulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vmaVulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
        vmaVulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
        vmaVulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
        vmaVulkanFunctions.vkAllocateMemory = vkAllocateMemory;
        vmaVulkanFunctions.vkFreeMemory = vkFreeMemory;
        vmaVulkanFunctions.vkMapMemory = vkMapMemory;
        vmaVulkanFunctions.vkUnmapMemory = vkUnmapMemory;
        vmaVulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
        vmaVulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
        vmaVulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
        vmaVulkanFunctions.vkBindImageMemory = vkBindImageMemory;
        vmaVulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
        vmaVulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
        vmaVulkanFunctions.vkCreateBuffer = vkCreateBuffer;
        vmaVulkanFunctions.vkCreateImage = vkCreateImage;
        vmaVulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
        vmaVulkanFunctions.vkDestroyImage = vkDestroyImage;
        vmaVulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;

        vmaVulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
        vmaVulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR;
        vmaVulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2KHR;
        vmaVulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2KHR;
        vmaVulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR;
        vmaVulkanFunctions.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;
        vmaVulkanFunctions.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;

        allocatorCreateInfo.pVulkanFunctions = &vmaVulkanFunctions;
        allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;

        if (vmaCreateAllocator(&allocatorCreateInfo, &vmaAllocator) != VK_SUCCESS)
            throw love::Exception("Failed to create VMA allocator in libretro mode");
        
        // std::printf("[CHAILOVE DEBUG] VMA allocator created successfully: %p\n", vmaAllocator);
    }
    
    // Initialize command buffers if not already done
    if (commandBuffers.empty()) {
        // std::printf("[CHAILOVE DEBUG] Initializing command buffers with pool: %p\n", this->commandPool);
        // Initialize cleanup and callback vectors
        cleanUpFunctions.clear();
        cleanUpFunctions.resize(MAX_FRAMES_IN_FLIGHT);
        readbackCallbacks.clear();
        readbackCallbacks.resize(MAX_FRAMES_IN_FLIGHT);
        
        // Create command buffers from our command pool
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = this->commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
            throw love::Exception("Failed to allocate command buffers in libretro mode");
        
        // std::printf("[CHAILOVE DEBUG] Command buffers allocated successfully, size: %zu\n", commandBuffers.size());
    }
    
    // Initialize capabilities now that we have a valid physicalDevice
    initCapabilities();
    
    // std::printf("[CHAILOVE DEBUG] libretroMode set to: %s\n", libretroMode ? "true" : "false");
    return true;
}

void Graphics::initCapabilities()
{
    // std::printf("[CHAILOVE DEBUG] initCapabilities() called, libretroMode = %s\n", 
        //    libretroMode ? "true" : "false");
           
    // Always set basic capabilities first
    capabilities.features[FEATURE_MULTI_RENDER_TARGET_FORMATS] = true;
    capabilities.features[FEATURE_CLAMP_ZERO] = true;
    capabilities.features[FEATURE_CLAMP_ONE] = true;
    capabilities.features[FEATURE_LIGHTEN] = true;
    capabilities.features[FEATURE_FULL_NPOT] = true;
    capabilities.features[FEATURE_PIXEL_SHADER_HIGHP] = true;
    capabilities.features[FEATURE_SHADER_DERIVATIVES] = true;
    capabilities.features[FEATURE_GLSL3] = true;
    capabilities.features[FEATURE_GLSL4] = true;
    capabilities.features[FEATURE_INSTANCING] = true;
    capabilities.features[FEATURE_TEXEL_BUFFER] = true;
    capabilities.features[FEATURE_COPY_TEXTURE_TO_BUFFER] = true;
    capabilities.features[FEATURE_INDIRECT_DRAW] = true;
    static_assert(FEATURE_MAX_ENUM == 13, "Graphics::initCapabilities must be updated when adding a new graphics feature!");

    // Always set texture types first to prevent "textures not supported" errors
    capabilities.textureTypes[TEXTURE_2D] = true;
    capabilities.textureTypes[TEXTURE_2D_ARRAY] = true;
    capabilities.textureTypes[TEXTURE_VOLUME] = true;
    capabilities.textureTypes[TEXTURE_CUBE] = true; 

    // std::printf("[CHAILOVE DEBUG] Set TEXTURE_2D = %d to true, current value = %s\n", 
        //    TEXTURE_2D, capabilities.textureTypes[TEXTURE_2D] ? "true" : "false");

    // Safety check for physicalDevice
    if (physicalDevice == VK_NULL_HANDLE)
    {
        if (libretroMode) {
            // In libretro mode, set reasonable defaults if physicalDevice is not available
            capabilities.limits[LIMIT_POINT_SIZE] = 64.0;
            capabilities.limits[LIMIT_TEXTURE_SIZE] = 16384.0;
            capabilities.limits[LIMIT_TEXTURE_LAYERS] = 2048.0;
            capabilities.limits[LIMIT_VOLUME_TEXTURE_SIZE] = 2048.0;
            capabilities.limits[LIMIT_CUBE_TEXTURE_SIZE] = 16384.0;
            capabilities.limits[LIMIT_TEXEL_BUFFER_SIZE] = 65536.0;
            capabilities.limits[LIMIT_SHADER_STORAGE_BUFFER_SIZE] = 134217728.0;
            capabilities.limits[LIMIT_THREADGROUPS_X] = 65535.0;
            capabilities.limits[LIMIT_THREADGROUPS_Y] = 65535.0;
            capabilities.limits[LIMIT_THREADGROUPS_Z] = 65535.0;
            capabilities.limits[LIMIT_RENDER_TARGETS] = 8.0;
            capabilities.limits[LIMIT_TEXTURE_MSAA] = 4.0;
            capabilities.limits[LIMIT_ANISOTROPY] = 16.0;
            // std::printf("[CHAILOVE DEBUG] Using libretro defaults for capabilities limits\n");
            return;
        }
        else {
            // std::printf("[CHAILOVE DEBUG] physicalDevice is NULL in non-libretro mode!\n");
            return;
        }
    }

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    capabilities.limits[LIMIT_POINT_SIZE] = properties.limits.pointSizeRange[1];
    capabilities.limits[LIMIT_TEXTURE_SIZE] = properties.limits.maxImageDimension2D;
    capabilities.limits[LIMIT_TEXTURE_LAYERS] = properties.limits.maxImageArrayLayers;
    capabilities.limits[LIMIT_VOLUME_TEXTURE_SIZE] = properties.limits.maxImageDimension3D;
    capabilities.limits[LIMIT_CUBE_TEXTURE_SIZE] = properties.limits.maxImageDimensionCube;
    capabilities.limits[LIMIT_TEXEL_BUFFER_SIZE] = properties.limits.maxTexelBufferElements;
    capabilities.limits[LIMIT_SHADER_STORAGE_BUFFER_SIZE] = properties.limits.maxStorageBufferRange;
    capabilities.limits[LIMIT_THREADGROUPS_X] = properties.limits.maxComputeWorkGroupCount[0];
    capabilities.limits[LIMIT_THREADGROUPS_Y] = properties.limits.maxComputeWorkGroupCount[1];
    capabilities.limits[LIMIT_THREADGROUPS_Z] = properties.limits.maxComputeWorkGroupCount[2];
    capabilities.limits[LIMIT_RENDER_TARGETS] = properties.limits.maxColorAttachments;
    capabilities.limits[LIMIT_TEXTURE_MSAA] = static_cast<double>(getMsaaCount(64));
    capabilities.limits[LIMIT_ANISOTROPY] = properties.limits.maxSamplerAnisotropy;
    static_assert(LIMIT_MAX_ENUM == 13, "Graphics::initCapabilities must be updated when adding a new system limit!");
    
    // std::printf("[CHAILOVE DEBUG] initCapabilities completed, final TEXTURE_2D support = %s\n",
        //    capabilities.textureTypes[TEXTURE_2D] ? "true" : "false");
}

void Graphics::getAPIStats(int &shaderswitches) const
{
	shaderswitches = static_cast<int>(Vulkan::getNumShaderSwitches());
}

void Graphics::unSetMode()
{
	if (created)
		submitGpuCommands(SUBMIT_NOPRESENT);

	created = false;

	cleanupSwapChain();
	vkDestroySurfaceKHR(instance, surface, nullptr);
}

void Graphics::setActive(bool enable)
{
	flushBatchedDraws();
	active = enable;
}

int Graphics::getRequestedBackbufferMSAA() const
{
	return requestedMsaa;
}

int Graphics::getBackbufferMSAA() const
{
	return static_cast<int>(msaaSamples);
}

void Graphics::setFrontFaceWinding(Winding winding)
{
	const auto& currentState = states.back();

	if (currentState.winding == winding)
		return;

	flushBatchedDraws();

	states.back().winding = winding;

	if (optionalDeviceExtensions.extendedDynamicState)
		vkCmdSetFrontFaceEXT(
			commandBuffers.at(currentFrame),
			Vulkan::getFrontFace(winding));
}

void Graphics::setColorMask(ColorChannelMask mask)
{
	flushBatchedDraws();

	states.back().colorMask = mask;
}

void Graphics::setBlendState(const BlendState &blend)
{
	flushBatchedDraws();

	states.back().blend = blend;
}

void Graphics::setPointSize(float size)
{
	if (size != states.back().pointSize)
		flushBatchedDraws();

	states.back().pointSize = size;
}

bool Graphics::usesGLSLES() const
{
	return false;
}

Graphics::RendererInfo Graphics::getRendererInfo() const
{
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

	Graphics::RendererInfo info;

	info.name = "Vulkan";
	info.device = deviceProperties.deviceName;
	info.vendor = Vulkan::getVendorName(deviceProperties.vendorID);
	info.version = Vulkan::getVulkanApiVersion(deviceProperties.apiVersion);

	return info;
}

void Graphics::draw(const DrawCommand &cmd)
{
	prepareDraw(*cmd.attributes, *cmd.buffers, cmd.texture, cmd.primitiveType, cmd.cullMode);

	if (cmd.indirectBuffer != nullptr)
    {
        // Debug output for indirect draws
        // std::printf("[CHAILOVE DEBUG] Issuing vkCmdDrawIndirect - this should render geometry\n");
        vkCmdDrawIndirect(commandBuffers.at(currentFrame), (VkBuffer) cmd.indirectBuffer->getHandle(), cmd.indirectBufferOffset, 1, 0);
    }
    else
    {
        // Debug output for direct draws  
        // std::printf("[CHAILOVE DEBUG] Issuing vkCmdDraw: vertexCount=%d, instanceCount=%d\n", cmd.vertexCount, cmd.instanceCount);
        vkCmdDraw(commandBuffers.at(currentFrame), cmd.vertexCount, cmd.instanceCount, cmd.vertexStart, 0);
    }

	drawCalls++;
}

void Graphics::draw(const DrawIndexedCommand &cmd)
{
	prepareDraw(*cmd.attributes, *cmd.buffers, cmd.texture, cmd.primitiveType, cmd.cullMode);

	// CRITICAL SAFETY CHECK: Vulkan requires active render pass for draw commands
    if (!renderPassState.active) {
        // std::printf("[CHAILOVE ERROR] Cannot issue draw commands - no active render pass!\n");
        // std::printf("[CHAILOVE ERROR] This will cause RenderDoc crashes and validation errors\n");
        return;
    }

    // return;

	vkCmdBindIndexBuffer(
		commandBuffers.at(currentFrame),
		(VkBuffer) cmd.indexBuffer->getHandle(),
		(VkDeviceSize) cmd.indexBufferOffset,
		Vulkan::getVulkanIndexBufferType(cmd.indexType));

	if (cmd.indirectBuffer != nullptr)
	{
		// std::printf("[CHAILOVE DEBUG] Issuing vkCmdDrawIndexedIndirect\n");
		vkCmdDrawIndexedIndirect(
			commandBuffers.at(currentFrame),
			(VkBuffer) cmd.indirectBuffer->getHandle(),
			cmd.indirectBufferOffset,
			1,
			0);
	}
	else
	{
		// std::printf("[CHAILOVE DEBUG] Issuing vkCmdDrawIndexed: indexCount=%d, instanceCount=%d\n", cmd.indexCount, cmd.instanceCount);
		vkCmdDrawIndexed(
			commandBuffers.at(currentFrame),
			(uint32) cmd.indexCount,
			(uint32) cmd.instanceCount,
			0,
			0,
			0);
	}

	drawCalls++;
}

void Graphics::drawQuads(int start, int count, const VertexAttributes &attributes, const BufferBindings &buffers, gfx::Texture *texture)
{
	const int MAX_VERTICES_PER_DRAW = LOVE_UINT16_MAX;
	const int MAX_QUADS_PER_DRAW = MAX_VERTICES_PER_DRAW / 4;

	prepareDraw(attributes, buffers, texture, PRIMITIVE_TRIANGLES, CULL_NONE);

	vkCmdBindIndexBuffer(
		commandBuffers.at(currentFrame),
		(VkBuffer)quadIndexBuffer->getHandle(),
		0,
		Vulkan::getVulkanIndexBufferType(INDEX_UINT16));

	int baseVertex = start * 4;

	for (int quadindex = 0; quadindex < count; quadindex += MAX_QUADS_PER_DRAW)
	{
		int quadcount = std::min(MAX_QUADS_PER_DRAW, count - quadindex);

		vkCmdDrawIndexed(
			commandBuffers.at(currentFrame),
			static_cast<uint32_t>(quadcount * 6),
			1,
			0,
			baseVertex,
			0);
		baseVertex += quadcount * 4;

		drawCalls++;
	}
}

void Graphics::setColor(Colorf c)
{
	c.r = std::min(std::max(c.r, 0.0f), 1.0f);
	c.g = std::min(std::max(c.g, 0.0f), 1.0f);
	c.b = std::min(std::max(c.b, 0.0f), 1.0f);
	c.a = std::min(std::max(c.a, 0.0f), 1.0f);

	states.back().color = c;
}

void Graphics::applyScissor()
{
    VkRect2D scissor{};

    bool win = renderPassState.isWindow;
    
    // LIBRETRO FIX: In libretro mode, always use renderPassState dimensions
    if (libretroMode) {
        scissor.extent.width = static_cast<uint32_t>(renderPassState.width);
        scissor.extent.height = static_cast<uint32_t>(renderPassState.height);
    } else {
        scissor.extent.width = win ? swapChainExtent.width : renderPassState.width;
        scissor.extent.height = win ? swapChainExtent.height : renderPassState.height;
    }

    if (states.back().scissor)
    {
        const Rect &rect = states.back().scissorRect;
        double dpiScale = getCurrentDPIScale();

        int minScissorX = (int)(rect.x * dpiScale);
        int minScissorY = (int)(rect.y * dpiScale);

        int maxScissorX = minScissorX + (int)(rect.w * dpiScale) - 1;
        int maxScissorY = minScissorY + (int)(rect.h * dpiScale) - 1;

        // Avoid negative offsets.
        int minX = std::max(scissor.offset.x, minScissorX);
        int minY = std::max(scissor.offset.y, minScissorY);

        int maxX = std::min(scissor.offset.x + (int)scissor.extent.width - 1, maxScissorX);
        int maxY = std::min(scissor.offset.y + (int)scissor.extent.height - 1, maxScissorY);

        if (maxX >= minX && maxY >= minY)
        {
            scissor.offset.x = minX;
            scissor.offset.y = minY;
            scissor.extent.width = (maxX - minX) + 1;
            scissor.extent.height = (maxY - minY) + 1;
        }
        else
        {
            scissor.extent.width = 0;
            scissor.extent.height = 0;
        }
    }

    vkCmdSetScissor(commandBuffers.at(currentFrame), 0, 1, &scissor);
}

void Graphics::setScissor(const Rect &rect)
{
	flushBatchedDraws();

	states.back().scissor = true;
	states.back().scissorRect = rect;

	if (renderPassState.active)
		applyScissor();
}

void Graphics::setScissor()
{
	flushBatchedDraws();

	states.back().scissor = false;

	if (renderPassState.active)
		applyScissor();
}

void Graphics::setStencilState(const StencilState &s)
{
	validateStencilState(s);

	flushBatchedDraws();

	vkCmdSetStencilWriteMask(commandBuffers.at(currentFrame), VK_STENCIL_FRONT_AND_BACK, s.writeMask);
	
	vkCmdSetStencilCompareMask(commandBuffers.at(currentFrame), VK_STENCIL_FRONT_AND_BACK, s.readMask);
	vkCmdSetStencilReference(commandBuffers.at(currentFrame), VK_STENCIL_FRONT_AND_BACK, s.value);

	if (optionalDeviceExtensions.extendedDynamicState)
		vkCmdSetStencilOpEXT(
			commandBuffers.at(currentFrame),
			VK_STENCIL_FRONT_AND_BACK,
			VK_STENCIL_OP_KEEP, Vulkan::getStencilOp(s.action),
			VK_STENCIL_OP_KEEP, Vulkan::getCompareOp(getReversedCompareMode(s.compare)));

	states.back().stencil = s;
}

void Graphics::setDepthMode(CompareMode compare, bool write)
{
	validateDepthState(write);

	flushBatchedDraws();

	if (optionalDeviceExtensions.extendedDynamicState)
	{
		vkCmdSetDepthCompareOpEXT(
			commandBuffers.at(currentFrame), Vulkan::getCompareOp(compare));

		vkCmdSetDepthWriteEnableEXT(
			commandBuffers.at(currentFrame), Vulkan::getBool(write));
	}

	states.back().depthTest = compare;
	states.back().depthWrite = write;
}

void Graphics::setWireframe(bool enable)
{
	flushBatchedDraws();

	states.back().wireframe = enable;
}

bool Graphics::isPixelFormatSupported(PixelFormat format, uint32 usage)
{
    // Safety check for libretro mode - physicalDevice must be valid
    if (physicalDevice == VK_NULL_HANDLE)
    {
        // If we're in libretro mode and haven't been properly initialized yet, 
        // assume basic formats are supported to prevent crashes
        if (libretroMode)
        {
            switch (getSizedFormat(format))
            {
                case PIXELFORMAT_RGBA8_UNORM:
                case PIXELFORMAT_RGBA8_sRGB:
                case PIXELFORMAT_BGRA8_UNORM:
                case PIXELFORMAT_BGRA8_sRGB:
                case PIXELFORMAT_DEPTH24_UNORM_STENCIL8:
                case PIXELFORMAT_DEPTH32_FLOAT_STENCIL8:
                    return true;
                default:
                    return false;
            }
        }
        throw love::Exception("Cannot check pixel format support: physicalDevice is VK_NULL_HANDLE");
    }

    format = getSizedFormat(format);

    switch (format)
    {
    case PIXELFORMAT_PVR1_RGB2_UNORM:
    case PIXELFORMAT_PVR1_RGB2_sRGB:
    case PIXELFORMAT_PVR1_RGB4_UNORM:
    case PIXELFORMAT_PVR1_RGB4_sRGB:
    case PIXELFORMAT_PVR1_RGBA2_UNORM:
    case PIXELFORMAT_PVR1_RGBA2_sRGB:
    case PIXELFORMAT_PVR1_RGBA4_UNORM:
    case PIXELFORMAT_PVR1_RGBA4_sRGB:
        // Lets not support these in Vulkan - they're deprecated.
        return false;
    default:
        break;
    }

    auto vulkanFormat = Vulkan::getTextureFormat(format);

    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, vulkanFormat.internalFormat, &formatProperties);

    VkFormatFeatureFlags featureFlags = formatProperties.optimalTilingFeatures;
    VkImageUsageFlags usageFlags = 0;

    if (!featureFlags)
		return false;

	if (usage & PIXELFORMATUSAGEFLAGS_SAMPLE)
	{
		usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
		if (!(featureFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
			return false;
	}

	if (usage & PIXELFORMATUSAGEFLAGS_LINEAR)
	{
		if (!(featureFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
			return false;
	}

	if (usage & PIXELFORMATUSAGEFLAGS_RENDERTARGET)
	{
		if (isPixelFormatDepth(format) || isPixelFormatDepthStencil(format))
		{
			usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			if (!(featureFlags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
				return false;
		}
		else
		{
			usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			if (!(featureFlags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
				return false;
		}
	}

	if (usage & PIXELFORMATUSAGEFLAGS_BLEND)
	{
		if (!(featureFlags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT))
			return false;
	}

	if (usage & PIXELFORMATUSAGEFLAGS_COMPUTEWRITE)
	{
		usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
		if (!(featureFlags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
			return false;
	}

	if (usage & PIXELFORMATUSAGEFLAGS_MSAA)
	{
		VkImageFormatProperties properties;

		if (vkGetPhysicalDeviceImageFormatProperties(physicalDevice, vulkanFormat.internalFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usageFlags, 0, &properties) != VK_SUCCESS)
			return false;

		if (static_cast<uint32_t>(properties.sampleCounts) == 1)
			return false;
	}

	return true;
}

Renderer Graphics::getRenderer() const
{
	return RENDERER_VULKAN;
}

gfx::GraphicsReadback *Graphics::newReadbackInternal(ReadbackMethod method, love::gfx::Buffer *buffer, size_t offset, size_t size, datamod::ByteData *dest, size_t destoffset)
{
	return new GraphicsReadback(this, method, buffer, offset, size, dest, destoffset);
}

gfx::GraphicsReadback *Graphics::newReadbackInternal(ReadbackMethod method, love::gfx::Texture *texture, int slice, int mipmap, const Rect &rect, imagemod::ImageData *dest, int destx, int desty)
{
	return new GraphicsReadback(this, method, texture, slice, mipmap, rect, dest, destx, desty);
}

gfx::ShaderStage *Graphics::newShaderStageInternal(ShaderStageType stage, const std::string &cachekey, const std::string &source, bool gles)
{
	return new ShaderStage(this, stage, source, gles, cachekey);
}

gfx::Shader *Graphics::newShaderInternal(StrongRef<love::gfx::ShaderStage> stages[SHADERSTAGE_MAX_ENUM], const Shader::CompileOptions &options)
{
	return new Shader(stages, options);
}

gfx::StreamBuffer *Graphics::newStreamBuffer(BufferUsage type, size_t size)
{
	return new StreamBuffer(this, type, size);
}

static bool computeDispatchBarrierFlags(Shader *shader, VkAccessFlags &dstAccessFlags, VkPipelineStageFlags &dstStageFlags)
{
	for (const auto &info : shader->getActiveTextureInfo())
	{
		if ((info.access & Shader::ACCESS_WRITE) == 0)
			continue;

		if (info.texture == nullptr)
			return false;

		auto tex = (Texture *) info.texture;

		// All writable images use the GENERAL layout.
		// TODO: this is pretty messy.
		bool depthStencil  = isPixelFormatDepthStencil(tex->getPixelFormat());
		Vulkan::addImageLayoutTransitionOptions(false, tex->isRenderTarget(), depthStencil, VK_IMAGE_LAYOUT_GENERAL, dstAccessFlags, dstStageFlags);
	}

	for (const auto &info : shader->getActiveStorageBufferInfo())
	{
		if ((info.access & Shader::ACCESS_WRITE) == 0)
			continue;

		if (info.buffer == nullptr)
			return false;

		auto b = (Buffer *) info.buffer;
		dstAccessFlags |= b->getBarrierDstAccessFlags();
		dstStageFlags |= b->getBarrierDstStageFlags();
	}

	return true;
}

bool Graphics::dispatch(love::gfx::Shader *shader, int x, int y, int z)
{
	auto computeShader = (Shader *) shader;

	VkMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	VkPipelineStageFlags dstStageMask = 0;
	if (!computeDispatchBarrierFlags(computeShader, barrier.dstAccessMask, dstStageMask))
		return false;

	usedShadersInFrame.insert(computeShader);

	if (renderPassState.active)
		endRenderPass();

	vkCmdBindPipeline(commandBuffers.at(currentFrame), VK_PIPELINE_BIND_POINT_COMPUTE, computeShader->getComputePipeline());

	computeShader->cmdPushDescriptorSets(commandBuffers.at(currentFrame), VK_PIPELINE_BIND_POINT_COMPUTE);

	vkCmdDispatch(commandBuffers.at(currentFrame), (uint32) x, (uint32) y, (uint32) z);

	// Image layout transitions aren't needed, every writable image will be in the GENERAL layout.
	if (barrier.dstAccessMask != 0 || dstStageMask != 0)
		vkCmdPipelineBarrier(commandBuffers.at(currentFrame), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, dstStageMask, 0, 1, &barrier, 0, nullptr, 0, nullptr);

	return true;
}

bool Graphics::dispatch(love::gfx::Shader *shader, love::gfx::Buffer *indirectargs, size_t argsoffset)
{
	auto computeShader = (Shader *) shader;

	VkMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	VkPipelineStageFlags dstStageMask = 0;
	if (!computeDispatchBarrierFlags(computeShader, barrier.dstAccessMask, dstStageMask))
		return false;

	usedShadersInFrame.insert(computeShader);

	if (renderPassState.active)
		endRenderPass();

	vkCmdBindPipeline(commandBuffers.at(currentFrame), VK_PIPELINE_BIND_POINT_COMPUTE, computeShader->getComputePipeline());

	computeShader->cmdPushDescriptorSets(commandBuffers.at(currentFrame), VK_PIPELINE_BIND_POINT_COMPUTE);

	vkCmdDispatchIndirect(commandBuffers.at(currentFrame), (VkBuffer) indirectargs->getHandle(), argsoffset);

	// Image layout transitions aren't needed, every writable image will be in the GENERAL layout.
	if (barrier.dstAccessMask != 0 || dstStageMask != 0)
		vkCmdPipelineBarrier(commandBuffers.at(currentFrame), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, dstStageMask, 0, 1, &barrier, 0, nullptr, 0, nullptr);

	return true;
}

void Graphics::setRenderTargetsInternal(const RenderTargets &rts, int pixelw, int pixelh, bool hasSRGBtexture)
{
	if (renderPassState.active)
		endRenderPass();

	bool isWindow = rts.getFirstTarget().texture == nullptr;
	if (isWindow)
		setDefaultRenderPass();
	else
		setRenderPass(rts, pixelw, pixelh);
}

// END IMPLEMENTATION OVERRIDDEN FUNCTIONS

void Graphics::initDynamicState()
{
	vkCmdSetStencilWriteMask(commandBuffers.at(currentFrame), VK_STENCIL_FRONT_AND_BACK, states.back().stencil.writeMask);
	vkCmdSetStencilCompareMask(commandBuffers.at(currentFrame), VK_STENCIL_FRONT_AND_BACK, states.back().stencil.readMask);
	vkCmdSetStencilReference(commandBuffers.at(currentFrame), VK_STENCIL_FRONT_AND_BACK, states.back().stencil.value);

	if (optionalDeviceExtensions.extendedDynamicState)
	{
		 vkCmdSetStencilOpEXT(
			commandBuffers.at(currentFrame),
			VK_STENCIL_FRONT_AND_BACK,
			VK_STENCIL_OP_KEEP, Vulkan::getStencilOp(states.back().stencil.action),
			VK_STENCIL_OP_KEEP, Vulkan::getCompareOp(getReversedCompareMode(states.back().stencil.compare)));

		vkCmdSetDepthCompareOpEXT(
			commandBuffers.at(currentFrame), Vulkan::getCompareOp(states.back().depthTest));

		vkCmdSetDepthWriteEnableEXT(
			commandBuffers.at(currentFrame), Vulkan::getBool(states.back().depthWrite));

		vkCmdSetFrontFaceEXT(
			commandBuffers.at(currentFrame), Vulkan::getFrontFace(states.back().winding));
	}
}

void Graphics::beginFrame()
{
	// vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

	if (swapChain != VK_NULL_HANDLE)
	{
		while (true)
		{
			VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
			if (result == VK_ERROR_OUT_OF_DATE_KHR)
			{
				recreateSwapChain();
				continue;
			}
			else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
				throw love::Exception("failed to acquire swap chain image");

			break;
		}

		imageRequested = true;
	}
	else
	{
		imageRequested = false;
	}

	for (auto &readbackCallback : readbackCallbacks.at(currentFrame))
		readbackCallback();
	readbackCallbacks.at(currentFrame).clear();

	for (auto &cleanUpFn : cleanUpFunctions.at(currentFrame))
		cleanUpFn();
	cleanUpFunctions.at(currentFrame).clear();

	startRecordingGraphicsCommands();

	if (!swapChainImages.empty())
	{
		Vulkan::cmdTransitionImageLayout(
			commandBuffers.at(currentFrame),
			swapChainImages[imageIndex],
			swapChainPixelFormat, true,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	}

	if (transitionColorDepthLayouts)
	{
		if (depthImage)
			Vulkan::cmdTransitionImageLayout(
				commandBuffers.at(currentFrame),
				depthImage,
				depthStencilPixelFormat, true,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

		if (colorImage)
			Vulkan::cmdTransitionImageLayout(
				commandBuffers.at(currentFrame),
				colorImage,
				swapChainPixelFormat, true,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		transitionColorDepthLayouts = false;
	}

	Vulkan::resetShaderSwitches();

	for (const auto &shader : usedShadersInFrame)
		shader->newFrame();
	usedShadersInFrame.clear();

	localUniformBuffer->nextFrame();
}

void Graphics::startRecordingGraphicsCommands()
{
    // Safety check for command buffers
    if (commandBuffers.empty() || currentFrame >= commandBuffers.size()) {
        throw love::Exception("Command buffers not properly initialized - size: %zu, currentFrame: %zu", 
                             commandBuffers.size(), currentFrame);
    }
    
    // Add debug information about the command buffer state
    VkCommandBuffer currentCommandBuffer = commandBuffers.at(currentFrame);
    // std::printf("[CHAILOVE DEBUG] startRecordingGraphicsCommands:\n");
    // std::printf("[CHAILOVE DEBUG] - currentFrame: %zu\n", currentFrame);
    // std::printf("[CHAILOVE DEBUG] - commandBuffers.size(): %zu\n", commandBuffers.size());
    // std::printf("[CHAILOVE DEBUG] - currentCommandBuffer: %p\n", currentCommandBuffer);
    // std::printf("[CHAILOVE DEBUG] - commandPool: %p\n", commandPool);
    // std::printf("[CHAILOVE DEBUG] - device: %p\n", device);
    // std::printf("[CHAILOVE DEBUG] - commandBufferRecording: %s\n", commandBufferRecording ? "true" : "false");
    
    // Check if command buffer is valid
    if (currentCommandBuffer == VK_NULL_HANDLE) {
        throw love::Exception("Command buffer at frame %zu is VK_NULL_HANDLE", currentFrame);
    }
    
    // IMPORTANT FIX: Reset the command buffer if it's already recording
    if (commandBufferRecording) {
        // std::printf("[CHAILOVE DEBUG] Command buffer already recording, resetting it first\n");
        
        // End the current recording if it's active
        VkResult endResult = vkEndCommandBuffer(currentCommandBuffer);
        if (endResult != VK_SUCCESS) {
            // std::printf("[CHAILOVE DEBUG] Warning: vkEndCommandBuffer failed with result: %d\n", endResult);
        }
        
        // Reset the command buffer to initial state
        VkResult resetResult = vkResetCommandBuffer(currentCommandBuffer, 0);
        if (resetResult != VK_SUCCESS) {
            // std::printf("[CHAILOVE DEBUG] vkResetCommandBuffer failed with result: %d\n", resetResult);
            throw love::Exception("Failed to reset command buffer (VkResult: %d)", resetResult);
        }
        
        commandBufferRecording = false;
        // std::printf("[CHAILOVE DEBUG] Command buffer reset successfully\n");
    }
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;
    beginInfo.pNext = nullptr;  // Explicitly set to nullptr for clarity

    // std::printf("[CHAILOVE DEBUG] About to call vkBeginCommandBuffer\n");
    // std::printf("[CHAILOVE DEBUG] - beginInfo.sType: %u\n", beginInfo.sType);
    // std::printf("[CHAILOVE DEBUG] - beginInfo.flags: %u\n", beginInfo.flags);
    // std::printf("[CHAILOVE DEBUG] - beginInfo.pNext: %p\n", beginInfo.pNext);
    // std::printf("[CHAILOVE DEBUG] - beginInfo.pInheritanceInfo: %p\n", beginInfo.pInheritanceInfo);

    VkResult result = vkBeginCommandBuffer(currentCommandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        // std::printf("[CHAILOVE DEBUG] vkBeginCommandBuffer failed with result: %d\n", result);
        throw love::Exception("failed to begin recording command buffer (VkResult: %d)", result);
    }

    // std::printf("[CHAILOVE DEBUG] vkBeginCommandBuffer succeeded\n");
    commandBufferRecording = true;

    initDynamicState();

    // This must be done after vkBeginCommandBuffer (since newTexture needs an
    // active command buffer for layout transitions), and before setDefaultRenderPass
    // (since that tries to use fakeBackbuffer).
    if (swapChainImages.empty() && fakeBackbuffer == nullptr)
    {
        // Create our render target texture for libretro
        auto settings = love::gfx::Texture::Settings{
                    width, height, 1,
                    love::gfx::TEXTURE_2D,
                    love::gfx::Texture::MipmapsMode::MIPMAPS_NONE,
                    0,
                    love::PixelFormat::PIXELFORMAT_NORMAL,
                    false,
                    1.0f,
                    1,
                    true  // renderTarget
                };
        fakeBackbuffer.set(new Texture(this, settings, nullptr), Acquire::NORETAIN);
    }

    setDefaultRenderPass();

    // For libretro mode, automatically request a clear with background color
    // if (libretroMode && !renderPassState.windowClearRequested) {
    //     renderPassState.windowClearRequested = true;
    //     // Use the current background color - convert from Colorf to ColorT<double>
    //     renderPassState.mainWindowClearColorValue.hasValue = true;
    //     auto bgColor = getBackgroundColor();
    //     renderPassState.mainWindowClearColorValue.value = ColorD(bgColor.r, bgColor.g, bgColor.b, bgColor.a);
    //     renderPassState.mainWindowClearDepthValue.hasValue = true;
    //     renderPassState.mainWindowClearDepthValue.value = 1.0;
    //     renderPassState.mainWindowClearStencilValue.hasValue = true;
    //     renderPassState.mainWindowClearStencilValue.value = 0;
        
    //     // std::printf("[CHAILOVE DEBUG] Auto-requesting clear for libretro mode with background color\n");
    // }

    if (defaultVertexBuffer)
    {
        VkBuffer vkBuffer = (VkBuffer) defaultVertexBuffer->getHandle();
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffers.at(currentFrame), DEFAULT_VERTEX_BUFFER_BINDING, 1, &vkBuffer, &offset);
    }
}

void Graphics::endRecordingGraphicsCommands()
{
	if (renderPassState.active)
		endRenderPass();

	if (vkEndCommandBuffer(commandBuffers.at(currentFrame)) != VK_SUCCESS)
		throw love::Exception("failed to record command buffer");
	
	commandBufferRecording = false;
}

void Graphics::setPushConstants(VkPipelineLayout pipelineLayout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *data)
{
    // std::printf("Pushing %u bytes to push constant at offset %u\n", size, offset);
    // for (uint32_t i = 0; i < size / 4; ++i)
    //     std::printf("%08x ", ((uint32_t*)data)[i]);
    // std::printf("\n");
    vkCmdPushConstants(
        commandBuffers.at(currentFrame),
        pipelineLayout,
        stageFlags,
        offset,
        size,
        data);
}

VkCommandBuffer Graphics::getCommandBufferForDataTransfer()
{
	if (renderPassState.active)
		endRenderPass();

	if (!commandBufferRecording) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;

        if (vkBeginCommandBuffer(commandBuffers.at(currentFrame), &beginInfo) != VK_SUCCESS)
            throw love::Exception("failed to begin recording command buffer for data transfer");
        
        commandBufferRecording = true;
        initDynamicState();  // Initialize dynamic state when starting recording
    }

	return commandBuffers.at(currentFrame);
}

void Graphics::queueCleanUp(std::function<void()> cleanUp)
{
	cleanUpFunctions.at(currentFrame).push_back(cleanUp);
}

void Graphics::addReadbackCallback(std::function<void()> callback)
{
	readbackCallbacks.at(currentFrame).push_back(callback);
}

gfx::Shader::BuiltinUniformData Graphics::getCurrentBuiltinUniformData()
{
	love::gfx::Shader::BuiltinUniformData data;

	data.transformMatrix = getTransform();
	data.projectionMatrix = getDeviceProjection();

	data.scaleParams.x = (float) getCurrentDPIScale();
	data.scaleParams.y = getPointSize();

	// Flip y to convert input y-up [-1, 1] to vulkan's y-down [-1, 1].
	// Convert input z [-1, 1] to vulkan [0, 1].
	uint32 flags = Shader::CLIP_TRANSFORM_FLIP_Y | Shader::CLIP_TRANSFORM_Z_NEG1_1_TO_0_1;
	data.clipSpaceParams = Shader::computeClipSpaceParams(flags);

	const auto &rt = states.back().renderTargets.getFirstTarget();
	if (rt.texture != nullptr)
	{
		data.screenSizeParams.x = rt.texture->getPixelWidth(rt.mipmap);
		data.screenSizeParams.y = rt.texture->getPixelHeight(rt.mipmap);
	}
	else
	{
		data.screenSizeParams.x = getPixelWidth();
		data.screenSizeParams.y = getPixelHeight();
	}

	data.screenSizeParams.z = 1.0f;
	data.screenSizeParams.w = 0.0f;

	data.constantColor = getColor();
	gammaCorrectColor(data.constantColor);

	return data;
}

const OptionalDeviceExtensions &Graphics::getEnabledOptionalDeviceExtensions() const
{
	return optionalDeviceExtensions;
}

const OptionalInstanceExtensions &Graphics::getEnabledOptionalInstanceExtensions() const
{
	return optionalInstanceExtensions;
}

bool Graphics::checkValidationSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char *layerName : validationLayers)
	{
		bool layerFound = false;

		for (const auto &layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
			return false;
	}

	return true;
}

void Graphics::pickPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0)
		throw love::Exception("failed to find GPUs with Vulkan support");

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	std::multimap<int, VkPhysicalDevice> candidates;

	for (const auto &device : devices)
	{
		int score = rateDeviceSuitability(device);
		candidates.insert(std::make_pair(score, device));
	}

	if (candidates.rbegin()->first > 0)
		physicalDevice = candidates.rbegin()->second;
	else
		throw love::Exception("failed to find a suitable gpu");

	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);
	minUniformBufferOffsetAlignment = properties.limits.minUniformBufferOffsetAlignment;
	deviceApiVersion = properties.apiVersion;

	depthStencilFormat = findDepthFormat();
	switch (depthStencilFormat)
	{
    case VK_FORMAT_D32_SFLOAT:
        depthStencilPixelFormat = PIXELFORMAT_DEPTH32_FLOAT;
        break;
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		depthStencilPixelFormat = PIXELFORMAT_DEPTH32_FLOAT_STENCIL8;
		break;
	case VK_FORMAT_D24_UNORM_S8_UINT:
		depthStencilPixelFormat = PIXELFORMAT_DEPTH24_UNORM_STENCIL8;
		break;
	default:
		throw love::Exception("Failed to convert vulkan depth/stencil swapchain pixel format %d to love PixelFormat.", depthStencilFormat);
		break;
	}
}

bool Graphics::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	for (const auto &extension : availableExtensions)
		requiredExtensions.erase(extension.extensionName);

	return requiredExtensions.empty();
}

// if the score is nonzero then the device is suitable.
// A higher rating means generally better performance
// if the score is 0 the device is unsuitable
int Graphics::rateDeviceSuitability(VkPhysicalDevice device)
{
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	int score = 1;

	// optional

	// if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	// 	score += isLowPowerPreferred() ? 100 : 1000;
	// if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
	// 	score += isLowPowerPreferred() ? 1000 : 100;
	// if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
	// 	score += 10;

	// definitely needed

	QueueFamilyIndices indices = findQueueFamilies(device);
	if (!indices.isComplete())
		score = 0;

	bool extensionsSupported = checkDeviceExtensionSupport(device);
	if (!extensionsSupported)
		score = 0;

	if (extensionsSupported)
	{
		auto swapChainSupport = querySwapChainSupport(device);
		bool swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		if (!swapChainAdequate)
			score = 0;
	}

	if (!deviceFeatures.samplerAnisotropy)
		score = 0;

	if (!deviceFeatures.fillModeNonSolid)
		score = 0;

	return score;
}

QueueFamilyIndices Graphics::findQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto &queueFamily : queueFamilies)
	{
		if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT))
			indices.graphicsFamily = i;

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

		if (presentSupport)
			indices.presentFamily = i;

		if (indices.isComplete())
			break;

		i++;
	}

	return indices;
}

static void findOptionalDeviceExtensions(VkPhysicalDevice physicalDevice, OptionalDeviceExtensions &optionalDeviceExtensions)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

	for (const auto &extension : availableExtensions)
	{
		if (strcmp(extension.extensionName, VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME) == 0)
			optionalDeviceExtensions.extendedDynamicState = true;
		if (strcmp(extension.extensionName, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) == 0)
			optionalDeviceExtensions.memoryRequirements2 = true;
		if (strcmp(extension.extensionName, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) == 0)
			optionalDeviceExtensions.dedicatedAllocation = true;
		if (strcmp(extension.extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME) == 0)
			optionalDeviceExtensions.memoryBudget = true;
		if (strcmp(extension.extensionName, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME) == 0)
			optionalDeviceExtensions.shaderFloatControls = true;
		if (strcmp(extension.extensionName, VK_KHR_SPIRV_1_4_EXTENSION_NAME) == 0)
			optionalDeviceExtensions.spirv14 = true;
	}
}

void Graphics::createLogicalDevice()
{
	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = {
		indices.graphicsFamily.value,
		indices.presentFamily.value
	};

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	findOptionalDeviceExtensions(physicalDevice, optionalDeviceExtensions);

	// sanity check for dependencies.

	if (optionalDeviceExtensions.extendedDynamicState && !optionalInstanceExtensions.physicalDeviceProperties2)
		optionalDeviceExtensions.extendedDynamicState = false;
	if (optionalDeviceExtensions.dedicatedAllocation && !optionalDeviceExtensions.memoryRequirements2)
		optionalDeviceExtensions.dedicatedAllocation = false;
	if (optionalDeviceExtensions.memoryBudget && !optionalInstanceExtensions.physicalDeviceProperties2)
		optionalDeviceExtensions.memoryBudget = false;
	if (optionalDeviceExtensions.spirv14 && !optionalDeviceExtensions.shaderFloatControls)
		optionalDeviceExtensions.spirv14 = false;
	if (optionalDeviceExtensions.spirv14 && deviceApiVersion < VK_API_VERSION_1_1)
		optionalDeviceExtensions.spirv14 = false;

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.fillModeNonSolid = VK_TRUE;

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = &deviceFeatures;

	std::vector<const char*> enabledExtensions(deviceExtensions.begin(), deviceExtensions.end());
	if (optionalDeviceExtensions.extendedDynamicState)
		enabledExtensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
	if (optionalDeviceExtensions.memoryRequirements2)
		enabledExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
	if (optionalDeviceExtensions.dedicatedAllocation)
		enabledExtensions.push_back(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
	if (optionalDeviceExtensions.memoryBudget)
		enabledExtensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
	if (optionalDeviceExtensions.shaderFloatControls)
		enabledExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
	if (optionalDeviceExtensions.spirv14)
		enabledExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
	if (deviceApiVersion >= VK_API_VERSION_1_1)
		enabledExtensions.push_back(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME);

	createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
	createInfo.ppEnabledExtensionNames = enabledExtensions.data();

	if (isDebugEnabled())
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}

	VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures{};
	extendedDynamicStateFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
	extendedDynamicStateFeatures.extendedDynamicState = VK_TRUE;
	extendedDynamicStateFeatures.pNext = nullptr;

	if (optionalDeviceExtensions.extendedDynamicState)
		createInfo.pNext = &extendedDynamicStateFeatures;

	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
		throw love::Exception("failed to create logical device");

	volkLoadDevice(device);

	vkGetDeviceQueue(device, indices.graphicsFamily.value, 0, &graphicsQueue);
	vkGetDeviceQueue(device, indices.presentFamily.value, 0, &presentQueue);
}

void Graphics::createPipelineCache()
{
	VkPipelineCacheCreateInfo cacheInfo{};
	cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

	if (vkCreatePipelineCache(device, &cacheInfo, nullptr, &pipelineCache) != VK_SUCCESS)
		throw love::Exception("could not create pipeline cache");
}

void Graphics::initVMA()
{
	if (libretroMode) {
        return;
    }

	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.vulkanApiVersion = deviceApiVersion;
	allocatorCreateInfo.physicalDevice = physicalDevice;
	allocatorCreateInfo.device = device;
	allocatorCreateInfo.instance = instance;

	// Default of 256 MB is a little too wasteful for most love games.
	// TODO: Tune this more.
	allocatorCreateInfo.preferredLargeHeapBlockSize = 128 * 1024 * 1024;

	VmaVulkanFunctions vulkanFunctions{};

	vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
	vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
	vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
	vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
	vulkanFunctions.vkFreeMemory = vkFreeMemory;
	vulkanFunctions.vkMapMemory = vkMapMemory;
	vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
	vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
	vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
	vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
	vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
	vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
	vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
	vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
	vulkanFunctions.vkCreateImage = vkCreateImage;
	vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
	vulkanFunctions.vkDestroyImage = vkDestroyImage;
	vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;

	vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
	vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR;
	vulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2KHR;
	vulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2KHR;
	vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR;
	vulkanFunctions.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;
	vulkanFunctions.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;

	allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

	if (vmaCreateAllocator(&allocatorCreateInfo, &vmaAllocator) != VK_SUCCESS)
		throw love::Exception("failed to create VMA allocator");
}
 
void Graphics::createSurface()
{
	auto window = Module::getInstance<love::windowmod::Window>(M_WINDOW);
	const void *handle = window->getHandle();
	if (!SDL_Vulkan_CreateSurface((SDL_Window*)handle, instance, &surface))
		throw love::Exception("failed to create window surface");
}

SwapChainSupportDetails Graphics::querySwapChainSupport(VkPhysicalDevice device)
{
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

void Graphics::createSwapChain()
{
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

	if (extent.width > 0 && extent.height > 0)
	{
		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
			imageCount = swapChainSupport.capabilities.maxImageCount;

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;

		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value, indices.presentFamily.value };

		if (indices.graphicsFamily.value != indices.presentFamily.value)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}

		createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		createInfo.compositeAlpha = chooseCompositeAlpha(swapChainSupport.capabilities);
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
			throw love::Exception("failed to create swap chain");

		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
	}
	else
	{
		// Use a fake backbuffer. Creation is deferred until startRecordingGraphicsCommands
		// because newTexture needs an active command buffer to do its initial
		// layout transitions.
		swapChainImages.clear();
		extent.width = std::max(1, pixelWidth);
		extent.height = std::max(1, pixelHeight);

		if (isGammaCorrect())
			surfaceFormat.format = VK_FORMAT_R8G8B8A8_SRGB;
		else
			surfaceFormat.format = VK_FORMAT_R8G8B8A8_UNORM;
	}

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;

	switch (swapChainImageFormat)
	{
	case VK_FORMAT_B8G8R8A8_SRGB:
		swapChainPixelFormat = PIXELFORMAT_BGRA8_sRGB;
		break;
	case VK_FORMAT_B8G8R8A8_UNORM:
		swapChainPixelFormat = PIXELFORMAT_BGRA8_UNORM;
		break;
	case VK_FORMAT_R8G8B8A8_SRGB:
		swapChainPixelFormat = PIXELFORMAT_RGBA8_sRGB;
		break;
	case VK_FORMAT_R8G8B8A8_UNORM:
		swapChainPixelFormat = PIXELFORMAT_RGBA8_UNORM;
		break;
	default:
		throw love::Exception("Failed to convert vulkan depth/stencil swapchain image format %d to love PixelFormat.", swapChainImageFormat);
		break;
	}
}

VkSurfaceFormatKHR Graphics::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats)
{
	std::vector<VkFormat> formatOrder;

	// TODO: turn off GammaCorrect if a sRGB format can't be found?
	// TODO: does every platform have these formats?
	if (isGammaCorrect())
	{
		formatOrder = {
			VK_FORMAT_B8G8R8A8_SRGB,
			VK_FORMAT_R8G8B8A8_SRGB,
		};
	}
	else
	{
		formatOrder = {
			VK_FORMAT_B8G8R8A8_UNORM,
			VK_FORMAT_R8G8B8A8_SNORM,
		};
	}

	for (const auto format : formatOrder)
	{
		for (const auto &availableFormat : availableFormats)
		{
			if (availableFormat.format == format && availableFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
				return availableFormat;
		}
	}
	
	return availableFormats[0];
}

VkPresentModeKHR Graphics::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes)
{
	const auto begin = availablePresentModes.begin();
	const auto end = availablePresentModes.end();

	switch (vsync)
	{
	case -1:
		if (std::find(begin, end, VK_PRESENT_MODE_FIFO_RELAXED_KHR) != end)
			return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
		else
			return VK_PRESENT_MODE_FIFO_KHR;
	case 0:
		// Mailbox mode might be better than immediate mode for a lot of people.
		// But on at least some systems it acts as if vsync is enabled
		// https://github.com/love2d/love/issues/1852
		// TODO: is that a bug in love's code or the graphics driver / compositor?
		// Should love expose mailbox mode in an API to users in some manner,
		// instead of trying to guess what to do?
		if (std::find(begin, end, VK_PRESENT_MODE_IMMEDIATE_KHR) != end)
			return VK_PRESENT_MODE_IMMEDIATE_KHR;
		else if (std::find(begin, end, VK_PRESENT_MODE_MAILBOX_KHR) != end)
			return VK_PRESENT_MODE_MAILBOX_KHR;
		else
			return VK_PRESENT_MODE_FIFO_KHR;
	default:
		// TODO: support for swap interval = 2, etc?
		return VK_PRESENT_MODE_FIFO_KHR;
	}
}

static uint32_t clampuint32_t(uint32_t value, uint32_t min, uint32_t max)
{
	if (value < min)
		return min;

	if (value > max)
		return max;

	return value;
}

VkExtent2D Graphics::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities)
{
	if (capabilities.currentExtent.width != UINT32_MAX)
		return capabilities.currentExtent;
	else
	{
		VkExtent2D actualExtent = {
			static_cast<uint32_t>(pixelWidth),
			static_cast<uint32_t>(pixelHeight)
		};

		actualExtent.width = clampuint32_t(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = clampuint32_t(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

VkCompositeAlphaFlagBitsKHR Graphics::chooseCompositeAlpha(const VkSurfaceCapabilitiesKHR &capabilities)
{
	if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
		return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
		return VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
	else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
		return VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
	else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
		return VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
	else
		throw love::Exception("failed to find composite alpha");
}

void Graphics::createImageViews()
{
	swapChainImageViews.resize(swapChainImages.size());

	for (size_t i = 0; i < swapChainImages.size(); i++)
	{
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapChainImages.at(i);
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapChainImageFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews.at(i)) != VK_SUCCESS)
			throw love::Exception("failed to create image views");
	}
}

VkFramebuffer Graphics::createFramebuffer(FramebufferConfiguration &configuration)
{
	std::vector<VkImageView> attachments;

	for (const auto &colorView : configuration.colorViews)
		attachments.push_back(colorView);

	if (configuration.staticData.depthView)
		attachments.push_back(configuration.staticData.depthView);

	// Resolve attachments after everything else to match createRenderPass.
	for (const auto &colorResolveView : configuration.colorResolveViews)
		attachments.push_back(colorResolveView);

	VkFramebufferCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	createInfo.renderPass = configuration.staticData.renderPass;
	createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	createInfo.pAttachments = attachments.data();
	createInfo.width = configuration.staticData.width;
	createInfo.height = configuration.staticData.height;
	createInfo.layers = 1;

	VkFramebuffer frameBuffer;
	if (vkCreateFramebuffer(device, &createInfo, nullptr, &frameBuffer) != VK_SUCCESS)
		throw love::Exception("failed to create framebuffer");
	return frameBuffer;
}

VkFramebuffer Graphics::getFramebuffer(FramebufferConfiguration &configuration)
{
	VkFramebuffer framebuffer;

	auto it = framebuffers.find(configuration);
	if (it != framebuffers.end())
		framebuffer = it->second;
	else
	{
		framebuffer = createFramebuffer(configuration);
		framebuffers[configuration] = framebuffer;
	}

	framebufferUsages[framebuffer] = true;

	return framebuffer;
}

void Graphics::cleanupFramebuffers(VkImageView imageView, PixelFormat format)
{
	bool depthstencil = isPixelFormatDepthStencil(format);

	for (auto it = framebuffers.begin(); it != framebuffers.end();)
	{
		bool foundView = false;

		if (depthstencil)
		{
			if (it->first.staticData.depthView == imageView)
				foundView = true;
		}
		else
		{
			for (VkImageView view : it->first.colorViews)
			{
				if (view == imageView)
				{
					foundView = true;
					break;
				}
			}
			if (!foundView)
			{
				for (VkImageView view : it->first.colorResolveViews)
				{
					if (view == imageView)
					{
						foundView = true;
						break;
					}
				}
			}
		}

		if (foundView)
		{
			vkDestroyFramebuffer(device, it->second, nullptr);
			it = framebuffers.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void Graphics::createDefaultShaders()
{
	for (int i = 0; i < Shader::STANDARD_MAX_ENUM; i++)
	{
		auto stype = (Shader::StandardShader)i;

		if (!Shader::standardShaders[i])
		{
			std::vector<std::string> stages;
			stages.push_back(Shader::getDefaultCode(stype, SHADERSTAGE_VERTEX));
			stages.push_back(Shader::getDefaultCode(stype, SHADERSTAGE_PIXEL));
			Shader::standardShaders[i] = newShader(stages, {});
		}
	}
}

VkRenderPass Graphics::createRenderPass(RenderPassConfiguration &configuration)
{
	VkSubpassDescription subPass{};
	subPass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	VkSubpassDependency beginDependency{};
	beginDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	beginDependency.dstSubpass = 0;
	beginDependency.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
	beginDependency.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	beginDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	beginDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkSubpassDependency endDependency{};
	endDependency.srcSubpass = 0;
	endDependency.dstSubpass = VK_SUBPASS_EXTERNAL;
	endDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	endDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	endDependency.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
	endDependency.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	std::vector<VkAttachmentDescription> attachments;
	std::vector<VkAttachmentReference> colorAttachmentRefs;
	std::vector<VkAttachmentReference> colorResolveAttachmentRefs;

	uint32_t attachment = 0;
	for (const auto &colorAttachment : configuration.colorAttachments)
	{
		VkAttachmentReference reference{};
		reference.attachment = attachment++;
		reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachmentRefs.push_back(reference);

		VkAttachmentDescription colorDescription{};
		colorDescription.format = colorAttachment.format;
		colorDescription.samples = colorAttachment.msaaSamples;
		colorDescription.loadOp = colorAttachment.loadOp;
		colorDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		if (colorAttachment.msaaSamples > 1)
		{
			colorDescription.initialLayout = colorAttachment.msaaLayout;
			colorDescription.finalLayout = colorAttachment.msaaLayout;
		}
		else
		{
			colorDescription.initialLayout = colorAttachment.layout;
			colorDescription.finalLayout = colorAttachment.layout;
		}
		attachments.push_back(colorDescription);

		// I had a TODO here, but I don't remember why...
		if (colorAttachment.layout != VK_IMAGE_LAYOUT_UNDEFINED)
		{
			Vulkan::addImageLayoutTransitionOptions(true, true, false, colorAttachment.layout, beginDependency.srcAccessMask, beginDependency.srcStageMask);
			Vulkan::addImageLayoutTransitionOptions(false, true, false, colorAttachment.layout, endDependency.dstAccessMask, endDependency.dstStageMask);
		}

		if (colorAttachment.msaaLayout != VK_IMAGE_LAYOUT_UNDEFINED)
		{
			Vulkan::addImageLayoutTransitionOptions(true, true, false, colorAttachment.msaaLayout, beginDependency.srcAccessMask, beginDependency.srcStageMask);
			Vulkan::addImageLayoutTransitionOptions(false, true, false, colorAttachment.msaaLayout, endDependency.dstAccessMask, endDependency.dstStageMask);
		}
	}

	subPass.colorAttachmentCount = static_cast<uint32_t>(configuration.colorAttachments.size());
	subPass.pColorAttachments = colorAttachmentRefs.data();

	VkAttachmentReference depthStencilAttachmentRef{};
	if (configuration.staticData.depthStencilAttachment.format != VK_FORMAT_UNDEFINED)
	{
		depthStencilAttachmentRef.attachment = attachment++;
		depthStencilAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		subPass.pDepthStencilAttachment = &depthStencilAttachmentRef;

		VkAttachmentDescription depthStencilAttachment{};
		depthStencilAttachment.format = configuration.staticData.depthStencilAttachment.format;
		depthStencilAttachment.samples = configuration.staticData.depthStencilAttachment.msaaSamples;
		depthStencilAttachment.loadOp = configuration.staticData.depthStencilAttachment.depthLoadOp;
		depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthStencilAttachment.stencilLoadOp = configuration.staticData.depthStencilAttachment.stencilLoadOp;
		depthStencilAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthStencilAttachment.initialLayout = configuration.staticData.depthStencilAttachment.layout;
		depthStencilAttachment.finalLayout = configuration.staticData.depthStencilAttachment.layout;
		attachments.push_back(depthStencilAttachment);

		Vulkan::addImageLayoutTransitionOptions(true, true, true, configuration.staticData.depthStencilAttachment.layout, beginDependency.srcAccessMask, beginDependency.srcStageMask);
		Vulkan::addImageLayoutTransitionOptions(false, true, true, configuration.staticData.depthStencilAttachment.layout, endDependency.dstAccessMask, endDependency.dstStageMask);
	}

	// Add resolve attachments after everything else to make pClearValues simpler to implement.
	if (configuration.staticData.resolve)
	{
		for (const auto &colorAttachment : configuration.colorAttachments)
		{
			VkAttachmentReference reference{};
			reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			if (colorAttachment.layout == VK_IMAGE_LAYOUT_UNDEFINED)
			{
				reference.attachment = VK_ATTACHMENT_UNUSED;
				colorResolveAttachmentRefs.push_back(reference);
			}
			else
			{
				reference.attachment = attachment++;
				colorResolveAttachmentRefs.push_back(reference);

				VkAttachmentDescription resolveDescription{};
				resolveDescription.format = colorAttachment.format;
				resolveDescription.samples = VK_SAMPLE_COUNT_1_BIT;
				resolveDescription.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				resolveDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				resolveDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				resolveDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				resolveDescription.initialLayout = colorAttachment.layout;
				resolveDescription.finalLayout = colorAttachment.layout;

				attachments.push_back(resolveDescription);
			}
		}

		subPass.pResolveAttachments = colorResolveAttachmentRefs.data();
	}

	std::array<VkSubpassDependency, 2> dependencies = { beginDependency, endDependency };

	VkRenderPassCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	createInfo.pAttachments = attachments.data();
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subPass;
	createInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	createInfo.pDependencies = dependencies.data();

	VkRenderPass renderPass;
	if (vkCreateRenderPass(device, &createInfo, nullptr, &renderPass) != VK_SUCCESS)
		throw love::Exception("failed to create render pass");

	return renderPass;
}

VkRenderPass Graphics::getRenderPass(RenderPassConfiguration &configuration)
{
	VkRenderPass renderPass;
	auto it = renderPasses.find(configuration);
	if (it != renderPasses.end())
		renderPass = it->second;
	else
	{
		renderPass = createRenderPass(configuration);
		renderPasses[configuration] = renderPass;
	}

	return renderPass;
}

void Graphics::createVulkanVertexFormat(
    Shader *shader,
    const VertexAttributes &attributes,
    std::vector<VkVertexInputBindingDescription> &bindingDescriptions,
    std::vector<VkVertexInputAttributeDescription> &attributeDescriptions)
{
    std::set<uint32_t> usedBuffers;
    
    // DEBUG: Print shader vertex attributes
    // std::printf("[VULKAN DEBUG] createVulkanVertexFormat:\n");
    // std::printf("[VULKAN DEBUG] - attributes.enableBits: 0x%X\n", attributes.enableBits);
    // std::printf("[VULKAN DEBUG] - shader vertex attributes:\n");
   
    for (const auto &pair : shader->getVertexAttributeIndices())
    {
        int i = pair.second.index;
        uint32 bit = 1u << i;
        
        // std::printf("[VULKAN DEBUG] Processing attribute %d ('%s'):\n", i, pair.first.c_str());
        // std::printf("[VULKAN DEBUG] - bit: 0x%X\n", bit);
        // std::printf("[VULKAN DEBUG] - enableBits & bit: 0x%X\n", attributes.enableBits & bit);

        VkVertexInputAttributeDescription attribdesc{};
        attribdesc.location = i;

        if (attributes.enableBits & bit)
        {
            // std::printf("[VULKAN DEBUG] - Using custom buffer (enabled)\n");
            const auto &attrib = attributes.attribs[i];

            int bufferbinding = VERTEX_BUFFER_BINDING_START + attrib.bufferIndex;
            // std::printf("[VULKAN DEBUG] - bufferIndex: %d, calculated binding: %d\n", 
                    //    attrib.bufferIndex, bufferbinding);

            attribdesc.binding = bufferbinding;
            attribdesc.offset = attrib.offsetFromVertex;
            attribdesc.format = Vulkan::getVulkanVertexFormat(attrib.getFormat());

            if (usedBuffers.find(bufferbinding) == usedBuffers.end())
            {
                usedBuffers.insert(bufferbinding);

                VkVertexInputBindingDescription bindingdesc{};
                bindingdesc.binding = bufferbinding;
                if (attributes.instanceBits & (1u << attrib.bufferIndex))
                    bindingdesc.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
                else
                    bindingdesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                bindingdesc.stride = attributes.bufferLayouts[attrib.bufferIndex].stride;
                bindingDescriptions.push_back(bindingdesc);
            }
        }
        else
        {
            // std::printf("[VULKAN DEBUG] - Using default buffer (NOT enabled)\n");
            attribdesc.binding = DEFAULT_VERTEX_BUFFER_BINDING;

            // Indices should match the creation parameters for defaultVertexBuffer.
            switch (pair.second.baseType)
            {
            case DATA_BASETYPE_INT:
                attribdesc.offset = defaultVertexBuffer->getDataMember(1).offset;
                attribdesc.format = Vulkan::getVulkanVertexFormat(DATAFORMAT_INT32_VEC4);
                break;
            case DATA_BASETYPE_UINT:
                attribdesc.offset = defaultVertexBuffer->getDataMember(1).offset;
                attribdesc.format = Vulkan::getVulkanVertexFormat(DATAFORMAT_UINT32_VEC4);
                break;
            case DATA_BASETYPE_FLOAT:
            default:
                if (i == ATTRIB_COLOR)
                    attribdesc.offset = defaultVertexBuffer->getDataMember(2).offset;
                else
                    attribdesc.offset = defaultVertexBuffer->getDataMember(0).offset;
                attribdesc.format = Vulkan::getVulkanVertexFormat(DATAFORMAT_FLOAT_VEC4);
                break;
            }

            if (usedBuffers.find(DEFAULT_VERTEX_BUFFER_BINDING) == usedBuffers.end())
            {
                usedBuffers.insert(DEFAULT_VERTEX_BUFFER_BINDING);

                VkVertexInputBindingDescription bindingdesc{};
                bindingdesc.binding = DEFAULT_VERTEX_BUFFER_BINDING;
                bindingdesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                bindingdesc.stride = 0; // no stride, will always read the same coord multiple times.
                bindingDescriptions.push_back(bindingdesc);
            }
        }

        attributeDescriptions.push_back(attribdesc);
    }
    
    // std::printf("[VULKAN DEBUG] Final result: %zu bindings, %zu attributes\n", 
    //            bindingDescriptions.size(), attributeDescriptions.size());
}

void Graphics::prepareDraw(VertexAttributes attributes, const BufferBindings &buffers, gfx::Texture *texture, PrimitiveType primitiveType, CullMode cullmode)
{
    // Add safety check for corrupted Graphics object
    if (this == nullptr) {
        // std::printf("[CHAILOVE ERROR] prepareDraw: Graphics object is null\n");
        return;
    }
    
    // Check if Graphics object is in a valid state
    try {
        if (magicNumber != GRAPHICS_MAGIC) {
            // std::printf("[CHAILOVE ERROR] prepareDraw: Graphics object corrupted (magic: 0x%08X, expected: 0x%08X)\n", 
            //            magicNumber, GRAPHICS_MAGIC);
            return;
        }
        
        if (!created) {
            // std::printf("[CHAILOVE ERROR] prepareDraw: Graphics object not created\n");
            return;
        }
    } catch (const std::exception& e) {
        // std::printf("[CHAILOVE ERROR] prepareDraw: Exception during validation: %s\n", e.what());
        return;
    } catch (...) {
        // std::printf("[CHAILOVE ERROR] prepareDraw: Unknown exception during validation\n");
        return;
    }

    // ADD VALIDATION CHECKS FOR VULKAN STATE
    // std::printf("[CHAILOVE DEBUG] prepareDraw validation:\n");
    // std::printf("[CHAILOVE DEBUG] - commandBufferRecording: %s\n", commandBufferRecording ? "true" : "false");
    // std::printf("[CHAILOVE DEBUG] - renderPassState.active: %s\n", renderPassState.active ? "true" : "false");
    
    // CRITICAL FIX: Start render pass automatically if none is active
	if (!renderPassState.active) {
		if (!commandBufferRecording) {
			// std::printf("[CHAILOVE DEBUG] prepareDraw: Starting command buffer recording\n");
			startRecordingGraphicsCommands();
		}else {
			// Command buffer is already recording, but we still need to set up render pass configuration
			// std::printf("[CHAILOVE DEBUG] prepareDraw: Command buffer already recording, setting up render pass configuration\n");
			setDefaultRenderPass();
		}

		// std::printf("[CHAILOVE DEBUG] prepareDraw: No active render pass, starting one automatically\n");
		startRenderPass();
		
		// Verify that render pass was successfully started
		if (!renderPassState.active) {
			// std::printf("[CHAILOVE ERROR] prepareDraw: Failed to start render pass!\n");
			return;
		}
		// std::printf("[CHAILOVE DEBUG] prepareDraw: Render pass started successfully\n");
	}
    
    // Validate command buffer
    VkCommandBuffer currentCommandBuffer = commandBuffers.at(currentFrame);
    if (currentCommandBuffer == VK_NULL_HANDLE) {
        // std::printf("[CHAILOVE ERROR] prepareDraw: Invalid command buffer!\n");
        return;
    }
    
    // ADD SHADER DEBUG OUTPUT
    // std::printf("[CHAILOVE DEBUG] prepareDraw shader status:\n");
    // std::printf("[CHAILOVE DEBUG] - Shader::current: %p\n", Shader::current);
    
    auto s = dynamic_cast<Shader*>(Shader::current);
    if (!s) {
        // std::printf("[CHAILOVE ERROR] prepareDraw: No valid shader is currently bound (current=%p)\n", Shader::current);
        
        // Try to use default shader
        if (Shader::standardShaders[Shader::STANDARD_DEFAULT]) {
            // std::printf("[CHAILOVE DEBUG] prepareDraw: Attempting to use default shader\n");
            s = dynamic_cast<Shader*>(Shader::standardShaders[Shader::STANDARD_DEFAULT]);
            if (s) {
                s->attach();
                // std::printf("[CHAILOVE DEBUG] prepareDraw: Default shader attached successfully\n");
            } else {
                // std::printf("[CHAILOVE ERROR] prepareDraw: Default shader cast failed\n");
                return;
            }
        } else {
            // std::printf("[CHAILOVE ERROR] prepareDraw: No default shader available\n");
            return;
        }
    } else {
        // std::printf("[CHAILOVE DEBUG] prepareDraw: Using shader %p\n", s);
    }

    usedShadersInFrame.insert(s);

    // LIBRETRO FIX: Handle pipeline creation when render pass is null
    GraphicsPipelineConfigurationFull configuration{};
    
    // If we don't have a render pass (direct rendering mode), we need to create one for pipeline compatibility
    VkRenderPass pipelineRenderPass = renderPassState.beginInfo.renderPass;
    
    if (pipelineRenderPass == VK_NULL_HANDLE) {
        // std::printf("[CHAILOVE ERROR] prepareDraw: No render pass available for pipeline creation!\n");
        
        // Try to start render pass
        if (!renderPassState.active) {
            // std::printf("[CHAILOVE DEBUG] prepareDraw: Attempting to start render pass\n");
            try {
                startRenderPass();
                pipelineRenderPass = renderPassState.beginInfo.renderPass;
                // std::printf("[CHAILOVE DEBUG] prepareDraw: Render pass started, new renderPass: %p\n", (void*)pipelineRenderPass);
            } catch (const std::exception& e) {
                // std::printf("[CHAILOVE ERROR] prepareDraw: Failed to start render pass: %s\n", e.what());
                return;
            }
        }
        
        if (pipelineRenderPass == VK_NULL_HANDLE) {
            // std::printf("[CHAILOVE ERROR] prepareDraw: Still no render pass after startRenderPass()\n");
            return;
        }
    }

    // VALIDATE RENDER PASS AND PIPELINE COMPATIBILITY
    // std::printf("[CHAILOVE DEBUG] Pipeline validation:\n");
    // std::printf("[CHAILOVE DEBUG] - pipelineRenderPass: %p\n", (void*)pipelineRenderPass);
    // std::printf("[CHAILOVE DEBUG] - renderPassState.numColorAttachments: %d\n", renderPassState.numColorAttachments);
    // std::printf("[CHAILOVE DEBUG] - renderPassState.msaa: %d\n", (int)renderPassState.msaa);
    
    configuration.core.renderPass = pipelineRenderPass;
    configuration.core.attributes = attributes;
    configuration.core.wireFrame = states.back().wireframe;
    configuration.core.blendStateKey = states.back().blend.toKey();
    configuration.core.colorChannelMask = states.back().colorMask;
    configuration.core.msaaSamples = renderPassState.msaa;
    configuration.core.numColorAttachments = renderPassState.numColorAttachments;
    configuration.core.packedColorAttachmentFormats = renderPassState.packedColorAttachmentFormats;
    configuration.core.primitiveType = primitiveType;
    configuration.core.depthWriteEnable = states.back().depthWrite;
    // if (states.back().depthTest == COMPARE_ALWAYS)
    configuration.core.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipeline pipeline = VK_NULL_HANDLE;

    try {
        pipeline = s->getCachedGraphicsPipeline(this, configuration);
        // std::printf("[CHAILOVE DEBUG] Pipeline retrieved successfully: %p\n", (void*)pipeline);
    } catch (const std::exception& e) {
        // std::printf("[CHAILOVE ERROR] Failed to get graphics pipeline: %s\n", e.what());
        return;
    }

    if (pipeline == VK_NULL_HANDLE) {
        // std::printf("[CHAILOVE ERROR] prepareDraw: Failed to create/get graphics pipeline\n");
        return;
    }

    if (pipeline != renderPassState.pipeline) {
        // std::printf("[CHAILOVE DEBUG] Binding new pipeline: %p (was %p)\n", (void*)pipeline, (void*)renderPassState.pipeline);
        vkCmdBindPipeline(commandBuffers.at(currentFrame), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        renderPassState.pipeline = pipeline;
    }

    // VALIDATE DESCRIPTOR SETS BEFORE BINDING
    // std::printf("[CHAILOVE DEBUG] Setting up descriptor sets\n");
    try {
        s->setMainTex(texture);
        s->cmdPushDescriptorSets(commandBuffers.at(currentFrame), VK_PIPELINE_BIND_POINT_GRAPHICS);
        // std::printf("[CHAILOVE DEBUG] Descriptor sets bound successfully\n");
    } catch (const std::exception& e) {
        // std::printf("[CHAILOVE ERROR] Failed to bind descriptor sets: %s\n", e.what());
        return;
    }

    VkBuffer vkbuffers[BufferBindings::MAX];
    VkDeviceSize vkoffsets[BufferBindings::MAX];
    uint32 buffercount = 0;

    uint32 allbits = buffers.useBits;
    uint32 i = 0;
    while (allbits) {
        if (allbits & 1) {
            if (buffers.info[i].buffer != nullptr) {
                vkbuffers[buffercount] = (VkBuffer)buffers.info[i].buffer->getHandle();
                vkoffsets[buffercount] = (VkDeviceSize)buffers.info[i].offset;
                buffercount++;
            }
        }
        allbits >>= 1;
        i++;
    }

    if (buffercount > 0) {
        // std::printf("[CHAILOVE DEBUG] Binding %u vertex buffers\n", buffercount);
        vkCmdBindVertexBuffers(commandBuffers.at(currentFrame), VERTEX_BUFFER_BINDING_START, buffercount, vkbuffers, vkoffsets);
    }
    
    // std::printf("[CHAILOVE DEBUG] prepareDraw completed successfully\n");
}

void Graphics::setDefaultRenderPass()
{
    // Add extensive pixel format debugging right at the beginning
    // std::printf("[CHAILOVE DEBUG] ========== PIXEL FORMAT DEBUG ==========\n");
    // std::printf("[CHAILOVE DEBUG] Checking supported pixel formats:\n");
    
    // Test key pixel formats
    std::vector<love::PixelFormat> testFormats = {
        love::PixelFormat::PIXELFORMAT_RGBA8_UNORM,
        love::PixelFormat::PIXELFORMAT_BGRA8_UNORM
    };
    
    for (auto format : testFormats) {
        bool supported = isPixelFormatSupported(format, PIXELFORMATUSAGEFLAGS_RENDERTARGET);
        // std::printf("[CHAILOVE DEBUG] Format %d supported: %s\n", (int)format, supported ? "YES" : "NO");
    }
    
    // Check fakeBackbuffer
    if (fakeBackbuffer) {
        // std::printf("[CHAILOVE DEBUG] fakeBackbuffer exists: %p\n", fakeBackbuffer.get());
        // Try to get format info from fakeBackbuffer if possible
    } else {
        // std::printf("[CHAILOVE DEBUG] fakeBackbuffer is null!\n");
    }
    // std::printf("[CHAILOVE DEBUG] ========================================\n");

    renderPassState.beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassState.beginInfo.renderPass = VK_NULL_HANDLE;
    renderPassState.beginInfo.framebuffer = VK_NULL_HANDLE;
    renderPassState.beginInfo.renderArea.offset = { 0, 0 };
    
    // For libretro mode, use fakeBackbuffer dimensions instead of swapchain
    uint32_t renderWidth = 0, renderHeight = 0;
    
    if (!swapChainImages.empty()) {
        renderWidth = swapChainExtent.width;
        renderHeight = swapChainExtent.height;
        // std::printf("[CHAILOVE DEBUG] Using swapchain dimensions: %ux%u\n", renderWidth, renderHeight);
    } else if (fakeBackbuffer) {
        renderWidth = static_cast<uint32_t>(width);
        renderHeight = static_cast<uint32_t>(height);
        // std::printf("[CHAILOVE DEBUG] Using fakeBackbuffer dimensions: %ux%u (from width=%d, height=%d)\n", 
                //    renderWidth, renderHeight, width, height);
    } else {
        renderWidth = 800;  // Fallback
        renderHeight = 600;
        // std::printf("[CHAILOVE DEBUG] Using fallback dimensions: %ux%u\n", renderWidth, renderHeight);
    }
    
    // Critical: Ensure dimensions are never zero
    if (renderWidth == 0 || renderHeight == 0) {
        renderWidth = 800;
        renderHeight = 600;
        // std::printf("[CHAILOVE DEBUG] Corrected zero dimensions to: %ux%u\n", renderWidth, renderHeight);
    }
    
    renderPassState.beginInfo.renderArea.extent.width = renderWidth;
    renderPassState.beginInfo.renderArea.extent.height = renderHeight;
    renderPassState.width = static_cast<float>(renderWidth);
    renderPassState.height = static_cast<float>(renderHeight);
    
    // std::printf("[CHAILOVE DEBUG] Final render area extent: %ux%u\n", renderWidth, renderHeight);

    renderPassState.isWindow = true;
    renderPassState.pipeline = VK_NULL_HANDLE;
    renderPassState.msaa = msaaSamples;
    renderPassState.numColorAttachments = 1;
    
    // CRITICAL FIX: Try multiple pixel formats until we find a supported one
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    
    if (!swapChainImages.empty()) {
        colorFormat = swapChainImageFormat;
        // std::printf("[CHAILOVE DEBUG] Using swapchain color format: %d\n", colorFormat);
    } else {
        // Try different formats in order of preference
        std::vector<VkFormat> candidateFormats = {
            VK_FORMAT_R8G8B8A8_UNORM,  // Most common
            VK_FORMAT_B8G8R8A8_UNORM,  // Alternative
            VK_FORMAT_R8G8B8A8_SRGB,   // sRGB variant
            VK_FORMAT_B8G8R8A8_SRGB    // sRGB alternative
        };
        
        for (auto format : candidateFormats) {
            // Test if this Vulkan format works
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
            
            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) {
                colorFormat = format;
                // std::printf("[CHAILOVE DEBUG] Selected working color format: %d\n", colorFormat);
                break;
            } else {
                // std::printf("[CHAILOVE DEBUG] Format %d not supported for color attachment\n", format);
            }
        }
        
        if (colorFormat == VK_FORMAT_UNDEFINED) {
            // std::printf("[CHAILOVE ERROR] No supported color format found! Using fallback.\n");
            colorFormat = VK_FORMAT_R8G8B8A8_UNORM; // Force fallback
        }
    }

    // Create render pass configuration
    RenderPassConfiguration renderPassConfiguration{};

    // CRITICAL FIX: Determine if we need depth/stencil and set appropriate load operations
    VkFormat dsformat = (backbufferHasDepth || backbufferHasStencil) ? depthStencilFormat : VK_FORMAT_UNDEFINED;
    
    // For the window render pass, we typically want to clear the color attachment
    // and load the depth/stencil if it exists (since it may contain useful data from previous frames)
    VkAttachmentLoadOp colorLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;  // Clear color each frame
    VkAttachmentLoadOp depthLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // Clear depth each frame  
    VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear stencil each frame
    
    ColorAttachment colorAttachment;
    colorAttachment.format = colorFormat;
    colorAttachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.msaaLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = colorLoadOp;
    colorAttachment.msaaSamples = msaaSamples;

    // Set up color attachment  
    renderPassConfiguration.colorAttachments.push_back(colorAttachment);

	renderPassState.packedColorAttachmentFormats = static_cast<uint64_t>(love::PixelFormat::PIXELFORMAT_NORMAL);

    // Rest of the function continues...
    // std::printf("[CHAILOVE DEBUG] ColorAttachment setup complete with format: %d\n", colorFormat);

    // Set up depth/stencil attachment if needed
    renderPassConfiguration.staticData.depthStencilAttachment = { 
        dsformat, 
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 
        depthLoadOp,
        stencilLoadOp, 
        msaaSamples 
    };
    
    // CRITICAL FIX: Calculate clear value count based on which attachments actually need clearing
    uint32_t numClearValues = 0;
    
    // Count color attachments that need clearing
    for (const auto& colorAtt : renderPassConfiguration.colorAttachments) {
        if (colorAtt.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
            numClearValues++;
        }
    }
    
    // Count depth/stencil if needed
    if (dsformat != VK_FORMAT_UNDEFINED && 
        (depthLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR || stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)) {
        numClearValues++;
    }
    
    // std::printf("[CHAILOVE DEBUG] Calculated numClearValues: %u (color attachments: %zu, depth format: %d)\n", 
            //    numClearValues, renderPassConfiguration.colorAttachments.size(), dsformat);
    
    // Set up clear values array with the correct size
    renderPassState.clearColors.resize(numClearValues);
    
    uint32_t clearIndex = 0;
    
    // Initialize color clear values
    for (size_t i = 0; i < renderPassConfiguration.colorAttachments.size(); i++) {
        if (renderPassConfiguration.colorAttachments[i].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
            renderPassState.clearColors[clearIndex].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            // std::printf("[CHAILOVE DEBUG] Set clear value [%u] for color attachment %zu\n", clearIndex, i);
            clearIndex++;
        }
    }
    
    // Initialize depth/stencil clear value if needed
    if (dsformat != VK_FORMAT_UNDEFINED && 
        (depthLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR || stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)) {
        renderPassState.clearColors[clearIndex].depthStencil = {1.0f, 0};
        // std::printf("[CHAILOVE DEBUG] Set clear value [%u] for depth/stencil attachment\n", clearIndex);
        clearIndex++;
    }
    
    // Set up the beginInfo with correct clear value count
    renderPassState.beginInfo.clearValueCount = numClearValues;
    renderPassState.beginInfo.pClearValues = numClearValues > 0 ? renderPassState.clearColors.data() : nullptr;
    
    // std::printf("[CHAILOVE DEBUG] Final clear setup: count=%u, pClearValues=%p\n", 
            //    renderPassState.beginInfo.clearValueCount, renderPassState.beginInfo.pClearValues);

    if (msaaSamples & VK_SAMPLE_COUNT_1_BIT)
        renderPassConfiguration.staticData.resolve = false;
    else
        renderPassConfiguration.staticData.resolve = true;

    FramebufferConfiguration framebufferConfiguration{};
    
    // Only set depth view if we have depth/stencil
    if (dsformat != VK_FORMAT_UNDEFINED && depthImageView != VK_NULL_HANDLE) {
        framebufferConfiguration.staticData.depthView = depthImageView;
    }
    
    framebufferConfiguration.staticData.width = renderWidth;
    framebufferConfiguration.staticData.height = renderHeight;

	if (msaaSamples & VK_SAMPLE_COUNT_1_BIT)
    {
        // No MSAA - use fakeBackbuffer directly
        if (!swapChainImages.empty()) {
            framebufferConfiguration.colorViews.push_back(swapChainImageViews[0]);
            // std::printf("[CHAILOVE DEBUG] Added swapchain color view: %p\n", swapChainImageViews[0]);
        } else if (fakeBackbuffer) {
            // Use fakeBackbuffer image view
            auto texture = dynamic_cast<Texture*>(fakeBackbuffer.get());
            if (texture) {
                VkImageView colorView = texture->getRenderTargetView(0, 0);
                // std::printf("[CHAILOVE DEBUG] fakeBackbuffer render target view: %p\n", colorView);
                
                if (colorView != VK_NULL_HANDLE) {
                    framebufferConfiguration.colorViews.push_back(colorView);
                    // std::printf("[CHAILOVE DEBUG] Successfully added fakeBackbuffer color view\n");
                } else {
                    // std::printf("[CHAILOVE ERROR] fakeBackbuffer render target view is VK_NULL_HANDLE!\n");
                    throw love::Exception("fakeBackbuffer render target view is invalid");
                }
            } else {
                // std::printf("[CHAILOVE ERROR] fakeBackbuffer is not a valid Texture!\n");
                throw love::Exception("Invalid fakeBackbuffer texture");
            }
        } else {
            // std::printf("[CHAILOVE ERROR] No color attachment available - no swapchain and no fakeBackbuffer!\n");
        }
    }
    else
    {
        // MSAA enabled - use color image as resolve target
        framebufferConfiguration.colorViews.push_back(colorImageView);
        // std::printf("[CHAILOVE DEBUG] Added MSAA color view: %p\n", colorImageView);
        
        if (!swapChainImages.empty()) {
            framebufferConfiguration.colorResolveViews.push_back(swapChainImageViews[0]);
        } else if (fakeBackbuffer) {
            auto texture = dynamic_cast<Texture*>(fakeBackbuffer.get());
            if (texture) {
                // Use the render-target view for resolve target (mipmap 0, slice 0)
                framebufferConfiguration.colorResolveViews.push_back(texture->getRenderTargetView(0, 0));
            }
        }
    }
    
    // CRITICAL DEBUG: Verify framebuffer configuration before using it
    // std::printf("[CHAILOVE DEBUG] Final framebuffer configuration:\n");
    // std::printf("[CHAILOVE DEBUG] - colorViews.size(): %zu\n", framebufferConfiguration.colorViews.size());
    // std::printf("[CHAILOVE DEBUG] - colorResolveViews.size(): %zu\n", framebufferConfiguration.colorResolveViews.size());
    // std::printf("[CHAILOVE DEBUG] - depthView: %p\n", framebufferConfiguration.staticData.depthView);
    
    if (framebufferConfiguration.colorViews.empty()) {
        // std::printf("[CHAILOVE ERROR] NO COLOR VIEWS IN FRAMEBUFFER - THIS WILL CAUSE DEPTH-ONLY RENDERING!\n");
        throw love::Exception("Framebuffer has no color attachments");
    }
    

    renderPassState.renderPassConfiguration = std::move(renderPassConfiguration);
    renderPassState.framebufferConfiguration = std::move(framebufferConfiguration);

    // Can't call clear() here because it depends on current RT state, which might not be
    // set yet when this is called from within setRenderTargetsInternal.
    if (renderPassState.windowClearRequested)
    {
        // std::printf("[CHAILOVE DEBUG] Window clear requested, setting color attachment to CLEAR\n");
        renderPassState.renderPassConfiguration.colorAttachments.at(0).loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        
        // Recalculate clear values since we changed the load operation
        uint32_t newNumClearValues = 0;
        
        // Count color attachments that need clearing
        for (const auto& colorAtt : renderPassState.renderPassConfiguration.colorAttachments) {
            if (colorAtt.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
                newNumClearValues++;
            }
        }
        
        // Count depth/stencil if needed
        if (dsformat != VK_FORMAT_UNDEFINED && 
            (depthLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR || stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)) {
            newNumClearValues++;
        }
        
        if (newNumClearValues != renderPassState.beginInfo.clearValueCount) {
            // std::printf("[CHAILOVE DEBUG] Updating clear value count from %u to %u due to window clear\n", 
                    //    renderPassState.beginInfo.clearValueCount, newNumClearValues);
            renderPassState.clearColors.resize(newNumClearValues);
            renderPassState.beginInfo.clearValueCount = newNumClearValues;
            renderPassState.beginInfo.pClearValues = newNumClearValues > 0 ? renderPassState.clearColors.data() : nullptr;
        }
    }
}

void Graphics::setRenderPass(const RenderTargets &rts, int pixelw, int pixelh)
{
	RenderPassConfiguration renderPassConfiguration{};
	VkSampleCountFlagBits msaa = VK_SAMPLE_COUNT_1_BIT;

	for (const auto &color : rts.colors)
	{
		auto tex = (Texture *)color.texture;
		renderPassConfiguration.colorAttachments.push_back({ 
			Vulkan::getTextureFormat(tex->getPixelFormat()).internalFormat,
			tex->getImageLayout(),
			tex->getMSAAImageLayout(),
			VK_ATTACHMENT_LOAD_OP_LOAD,
			tex->getMsaaSamples() });

		if (tex->getMSAAImageLayout() != VK_IMAGE_LAYOUT_UNDEFINED && tex->getImageLayout() != VK_IMAGE_LAYOUT_UNDEFINED)
			renderPassConfiguration.staticData.resolve = true;

		msaa = tex->getMsaaSamples();
	}

	if (rts.depthStencil.texture != nullptr)
	{
		auto tex = (Texture *)rts.depthStencil.texture;
		renderPassConfiguration.staticData.depthStencilAttachment = {
			Vulkan::getTextureFormat(rts.depthStencil.texture->getPixelFormat()).internalFormat,
			tex->getImageLayout(),
			VK_ATTACHMENT_LOAD_OP_LOAD,
			VK_ATTACHMENT_LOAD_OP_LOAD,
			tex->getMsaaSamples() };

		msaa = tex->getMsaaSamples();
	}

	FramebufferConfiguration configuration{};

	for (const auto &color : rts.colors)
	{
		auto tex = (Texture*)color.texture;
		if (tex->getMSAA() > 1)
		{
			configuration.colorViews.push_back(tex->getMSAARenderTargetView(color.mipmap, color.slice));
			configuration.colorResolveViews.push_back(tex->getRenderTargetView(color.mipmap, color.slice));
		}
		else
		{
			configuration.colorViews.push_back(tex->getRenderTargetView(color.mipmap, color.slice));
		}
	}
	if (rts.depthStencil.texture != nullptr)
	{
		auto tex = (Texture*)rts.depthStencil.texture;
		if (tex->getMSAA() > 1)
			configuration.staticData.depthView = tex->getMSAARenderTargetView(rts.depthStencil.mipmap, rts.depthStencil.slice);
		else
			configuration.staticData.depthView = tex->getRenderTargetView(rts.depthStencil.mipmap, rts.depthStencil.slice);
	}

	configuration.staticData.width = static_cast<uint32_t>(pixelw);
	configuration.staticData.height = static_cast<uint32_t>(pixelh);

	uint32_t numClearValues = static_cast<uint32_t>(rts.colors.size() + 1);
	renderPassState.clearColors.resize(numClearValues);

	renderPassState.beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassState.beginInfo.renderPass = VK_NULL_HANDLE;
	renderPassState.beginInfo.framebuffer = VK_NULL_HANDLE;
	renderPassState.beginInfo.renderArea.offset = {0, 0};
	renderPassState.beginInfo.renderArea.extent.width = static_cast<uint32_t>(pixelw);
	renderPassState.beginInfo.renderArea.extent.height = static_cast<uint32_t>(pixelh);
	renderPassState.beginInfo.clearValueCount = numClearValues;
	renderPassState.beginInfo.pClearValues = renderPassState.clearColors.data();

	renderPassState.isWindow = false;
	renderPassState.renderPassConfiguration = renderPassConfiguration;
	renderPassState.framebufferConfiguration = configuration;
	renderPassState.pipeline = VK_NULL_HANDLE;
	renderPassState.width = static_cast<float>(pixelw);
	renderPassState.height = static_cast<float>(pixelh);
	renderPassState.msaa = msaa;
	renderPassState.numColorAttachments = static_cast<uint32_t>(rts.colors.size());
	renderPassState.packedColorAttachmentFormats = 0;
	for (size_t i = 0; i < rts.colors.size(); i++)
		renderPassState.packedColorAttachmentFormats |= ((uint64)rts.colors[i].texture->getPixelFormat()) << (i * 8ull);
}

void Graphics::startRenderPass()
{
    if (renderPassState.active)
        return;

    // LIBRETRO COMPATIBILITY: Use minimal render pass setup
    if (libretroMode) {
        // std::printf("[CHAILOVE DEBUG] Using libretro-compatible rendering with minimal render pass\n");
        
        // CRITICAL FIX: Initialize render pass dimensions for libretro
        if (renderPassState.width <= 0 || renderPassState.height <= 0) {
            renderPassState.width = 1440.0f;  // Default RetroArch resolution
            renderPassState.height = 1080.0f;
            // std::printf("[CHAILOVE DEBUG] Initialized libretro render pass dimensions: %fx%f\n", 
                // renderPassState.width, renderPassState.height);
        }
        
        // CRITICAL: Create minimal render pass for pipeline compatibility
        if (renderPassState.beginInfo.renderPass == VK_NULL_HANDLE) {
            // std::printf("[CHAILOVE DEBUG] Creating minimal render pass for libretro\n");
            
            RenderPassConfiguration minimalConfig{};
            
            // Use RetroArch's expected format
            VkAttachmentDescription colorAttachment = {};
            colorAttachment.format = VK_FORMAT_B8G8R8A8_UNORM;
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkAttachmentDescription depthAttachment = {};
            depthAttachment.format = VK_FORMAT_D24_UNORM_S8_UINT;
            depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            minimalConfig.colorAttachments.push_back({
                colorAttachment.format,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                colorAttachment.loadOp,
                VK_SAMPLE_COUNT_1_BIT
            });
            minimalConfig.staticData.depthStencilAttachment = {
                depthAttachment.format,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                depthAttachment.loadOp,
                depthAttachment.stencilLoadOp,
                VK_SAMPLE_COUNT_1_BIT
            };

            VkRenderPass minimalRenderPass = getRenderPass(minimalConfig);
            renderPassState.beginInfo.renderPass = minimalRenderPass;
            
            // std::printf("[CHAILOVE DEBUG] Minimal render pass created: %p\n", (void*)minimalRenderPass);
        }
        
        // CRITICAL: Actually start the render pass for draw commands
        VkCommandBuffer currentCommandBuffer = commandBuffers.at(currentFrame);
        
        
                // Set up a dummy framebuffer for the render pass
        // In libretro mode, we don't actually render to this - RetroArch handles the real target
        if (renderPassState.beginInfo.framebuffer == VK_NULL_HANDLE) {
            // Create minimal framebuffer configuration
            FramebufferConfiguration fbConfig{};
            fbConfig.staticData.renderPass = renderPassState.beginInfo.renderPass;
            fbConfig.staticData.width = static_cast<uint32_t>(renderPassState.width);
            fbConfig.staticData.height = static_cast<uint32_t>(renderPassState.height);
            
            // Use fakeBackbuffer view if available
            if (fakeBackbuffer != nullptr) {
                VkImageView colorView = fakeBackbuffer->getRenderTargetView(0, 0);
                fbConfig.colorViews.push_back(colorView);
                fbConfig.staticData.depthView = depthImageView;
                // std::printf("[CHAILOVE DEBUG] Using fakeBackbuffer for framebuffer: %p\n", fakeBackbuffer.get());
            } else {
				// std::printf("[CHAILOVE ERROR] fakeBackbuffer is null! Creating dummy image for libretro compatibility\n");
					
					// Create a minimal dummy texture for the framebuffer directly using Texture::Settings
					Texture::Settings texSettings;
					texSettings.type = TEXTURE_2D;
					texSettings.format = love::PixelFormat::PIXELFORMAT_RGBA8_UNORM;
					texSettings.width = static_cast<int>(renderPassState.width);
					texSettings.height = static_cast<int>(renderPassState.height);
					texSettings.layers = 1;
					texSettings.mipmaps = love::gfx::Texture::MipmapsMode::MIPMAPS_NONE;
					texSettings.readable = false;
					texSettings.renderTarget = true;
					
					StrongRef<Texture> dummyTexture(new Texture(this, texSettings, nullptr), Acquire::NORETAIN);
					VkImageView colorView = dummyTexture->getRenderTargetView(0, 0);
					fbConfig.colorViews.push_back(colorView);

                    fbConfig.staticData.depthView = depthImageView;
					
					// std::printf("[CHAILOVE DEBUG] Created dummy texture for libretro framebuffer\n");
            }
            
            renderPassState.beginInfo.framebuffer = getFramebuffer(fbConfig);
            // std::printf("[CHAILOVE DEBUG] Minimal framebuffer created: %p with %zu color views\n", 
                // (void*)renderPassState.beginInfo.framebuffer, fbConfig.colorViews.size());
        }
        
        // Set up render area
        renderPassState.beginInfo.renderArea.offset = {0, 0};
        renderPassState.beginInfo.renderArea.extent.width = static_cast<uint32_t>(renderPassState.width);
        renderPassState.beginInfo.renderArea.extent.height = static_cast<uint32_t>(renderPassState.height);
        // Clear color, depth, and stencil values ARE NEEDED for proper operation
        // renderPassState.beginInfo.clearValueCount = 1;
        // VkClearValue clearValues[1];
        // clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // Clear color
        // clearValues[0].depthStencil = {1.0f, 0}; // Clear depth and stencil
        // renderPassState.clearColors[0] = clearValues[0];
        // renderPassState.beginInfo.pClearValues = clearValues;
               
        renderPassState.active = true;

        // if (renderPassState.isWindow && renderPassState.windowClearRequested)
        //     renderPassState.renderPassConfiguration.colorAttachments.at(0).loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

        // VkImageMemoryBarrier barrierToGeneralColor{};
        // barrierToGeneralColor.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        // barrierToGeneralColor.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // barrierToGeneralColor.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        // barrierToGeneralColor.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        // barrierToGeneralColor.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        // barrierToGeneralColor.image = fakeBackbuffer != nullptr ? 
        //                             reinterpret_cast<VkImage>(fakeBackbuffer->getRenderTargetHandle()) : 
        //                             VK_NULL_HANDLE;
        // barrierToGeneralColor.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        // barrierToGeneralColor.subresourceRange.baseMipLevel = 0;
        // barrierToGeneralColor.subresourceRange.levelCount = 1;
        // barrierToGeneralColor.subresourceRange.baseArrayLayer = 0;
        // barrierToGeneralColor.subresourceRange.layerCount = 1;
        // barrierToGeneralColor.srcAccessMask = 0;
        // barrierToGeneralColor.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        // vkCmdPipelineBarrier(currentCommandBuffer,
        //                      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        //                      VK_PIPELINE_STAGE_TRANSFER_BIT,
        //                      0,
        //                      0, nullptr,
        //                      0, nullptr,
        //                      1, &barrierToGeneralColor);
        // setDepthMode(gfx::CompareMode::COMPARE_LEQUAL, true);
        // gfx::OptionalColorD clearcolor;
        // OptionalInt clearstencil(0);
        // OptionalDouble cleardepth(1.0);
        // clear(clearcolor, clearstencil, cleardepth);

		// VkImageSubresourceRange colorSubresourceRange = {};
		// colorSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		// colorSubresourceRange.baseMipLevel = 0;
		// colorSubresourceRange.levelCount = 1;
		// colorSubresourceRange.baseArrayLayer = 0;
		// colorSubresourceRange.layerCount = 1;

		// vkCmdClearColorImage(currentCommandBuffer,
		// 					 fakeBackbuffer != nullptr ? 
		// 						reinterpret_cast<VkImage>(fakeBackbuffer->getRenderTargetHandle()) : 
		// 						VK_NULL_HANDLE,
		// 					 VK_IMAGE_LAYOUT_GENERAL,
		// 					 &renderPassState.clearColors[0].color,
		// 					 1,
		// 					 &colorSubresourceRange);

		// VkImageSubresourceRange depthSubresourceRange = {};
		// depthSubresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		// depthSubresourceRange.baseMipLevel = 0;
		// depthSubresourceRange.levelCount = 1;
		// depthSubresourceRange.baseArrayLayer = 0;
		// depthSubresourceRange.layerCount = 1;

		// vkCmdClearDepthStencilImage(currentCommandBuffer,
		// 							depthImage != VK_NULL_HANDLE ? depthImage : VK_NULL_HANDLE,
		// 							VK_IMAGE_LAYOUT_GENERAL,
		// 							&renderPassState.clearColors[1].depthStencil,
		// 							1,
		// 							&depthSubresourceRange);
        
        // vkCmdPipelineBarrier(currentCommandBuffer,
        //                      VK_PIPELINE_STAGE_TRANSFER_BIT,
        //                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        //                      0,
        //                      0, nullptr,
        //                      0, nullptr,
        //                      1, &barrierToGeneralColor);

        // CRITICAL: Actually begin the render pass
        // std::printf("[CHAILOVE DEBUG] Beginning minimal render pass for libretro\n");
        vkCmdBeginRenderPass(currentCommandBuffer, &renderPassState.beginInfo, VK_SUBPASS_CONTENTS_INLINE);
        
        // Set viewport
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = renderPassState.width;
        viewport.height = renderPassState.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(currentCommandBuffer, 0, 1, &viewport);
        
        // Apply scissor
        applyScissor();

        // std::printf("[CHAILOVE DEBUG] Libretro minimal render pass active\n");
        return;
    }

    // Original render pass logic for non-libretro mode
    // ... rest of original startRenderPass() code


    renderPassState.active = true;

    // if (renderPassState.isWindow && renderPassState.windowClearRequested)
    //     renderPassState.renderPassConfiguration.colorAttachments.at(0).loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

    // CRITICAL FIX: Ensure command buffers are properly initialized
    if (commandBuffers.empty() || currentFrame >= commandBuffers.size()) {
        // std::printf("[CHAILOVE ERROR] Command buffers not properly initialized!\n");
        throw love::Exception("Command buffers not available for rendering");
    }
    
    // Additional safety check
    VkCommandBuffer currentCommandBuffer = commandBuffers.at(currentFrame);
    if (currentCommandBuffer == VK_NULL_HANDLE) {
        // std::printf("[CHAILOVE ERROR] Current command buffer is VK_NULL_HANDLE at frame %zu\n", currentFrame);
        throw love::Exception("Command buffer is not valid");
    }

    // LIBRETRO-COMPATIBLE SOLUTION: Use simplified rendering approach
    // std::printf("[CHAILOVE DEBUG] Using libretro-compatible rendering approach\n");
    
    // Set viewport
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = renderPassState.width;
    viewport.height = renderPassState.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(currentCommandBuffer, 0, 1, &viewport);
    
    // FALLBACK: Direct rendering with manual clearing for libretro
    // std::printf("[CHAILOVE DEBUG] Using direct rendering with manual clearing for libretro compatibility\n");
    
    // Mark as active
    renderPassState.active = true;
    
    // CRITICAL FIX: Perform manual clearing using vkCmdClearColorImage
    if (renderPassState.isWindow && renderPassState.windowClearRequested) {
        // std::printf("[CHAILOVE DEBUG] Performing manual color clear for libretro\n");
        
        // Get the current swapchain image (in libretro, this should be RetroArch's image)
        VkImage targetImage = VK_NULL_HANDLE;
        
        // In libretro mode, we need to get the current render target image
        if (libretroMode && fakeBackbuffer != nullptr) {
            // Use fakeBackbuffer image - getRenderTargetHandle returns the VkImage cast to ptrdiff_t
            targetImage = reinterpret_cast<VkImage>(fakeBackbuffer->getRenderTargetHandle());
            // std::printf("[CHAILOVE DEBUG] Using fakeBackbuffer image: %p\n", (void*)targetImage);
        } else if (!swapChainImages.empty()) {
            // Use swapchain image
            targetImage = swapChainImages[imageIndex];
            // std::printf("[CHAILOVE DEBUG] Using swapchain image[%zu]: %p\n", imageIndex, (void*)targetImage);
        }
        
        if (false && targetImage != VK_NULL_HANDLE) {
            // Transition image to transfer destination layout
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = targetImage;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            
            vkCmdPipelineBarrier(currentCommandBuffer,
                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                0, 0, nullptr, 0, nullptr, 1, &barrier);
            
            // FORCE BRIGHT COLOR FOR TESTING: Clear with bright magenta to make it obvious
            VkClearColorValue clearColor = {};
            clearColor.float32[0] = 1.0f; // Red
            clearColor.float32[1] = 0.0f; // Green  
            clearColor.float32[2] = 1.0f; // Blue (magenta)
            clearColor.float32[3] = 1.0f; // Alpha
            
            // Also try to use the requested clear color if available
            if (!renderPassState.clearColors.empty()) {
                VkClearColorValue requestedColor = renderPassState.clearColors[0].color;
                // std::printf("[CHAILOVE DEBUG] Requested clear color: R=%.2f, G=%.2f, B=%.2f, A=%.2f\n",
                        //    requestedColor.float32[0], requestedColor.float32[1], requestedColor.float32[2], requestedColor.float32[3]);
                
                // Use the requested color instead of magenta
                clearColor = requestedColor;
            }
            
            // std::printf("[CHAILOVE DEBUG] Actually clearing with color: R=%.2f, G=%.2f, B=%.2f, A=%.2f\n",
                    //    clearColor.float32[0], clearColor.float32[1], clearColor.float32[2], clearColor.float32[3]);
            
            VkImageSubresourceRange range{};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseMipLevel = 0;
            range.levelCount = 1;
            range.baseArrayLayer = 0;
            range.layerCount = 1;
            
            vkCmdClearColorImage(currentCommandBuffer, targetImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
            
            // Transition back to color attachment layout
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            
            vkCmdPipelineBarrier(currentCommandBuffer,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                0, 0, nullptr, 0, nullptr, 1, &barrier);
                                
            // std::printf("[CHAILOVE DEBUG] Manual color clear completed\n");
        } else {
            // std::printf("[CHAILOVE WARNING] No target image available for clearing\n");
        }
    } else {
        // std::printf("[CHAILOVE DEBUG] Skipping clear: isWindow=%s, windowClearRequested=%s\n", 
                //    renderPassState.isWindow ? "true" : "false",
                //    renderPassState.windowClearRequested ? "true" : "false");
    }

	// VkRenderPass renderPass = getRenderPass(renderPassState.renderPassConfiguration);
	// renderPassState.beginInfo.renderPass = renderPass;
	// renderPassState.beginInfo.framebuffer = getFramebuffer(renderPassState.framebufferConfiguration);

	// vkCmdBeginRenderPass(currentCommandBuffer, &renderPassState.beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // Set up minimal beginInfo for compatibility
    renderPassState.beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassState.beginInfo.pNext = nullptr;
    renderPassState.beginInfo.renderPass = VK_NULL_HANDLE;
    renderPassState.beginInfo.framebuffer = VK_NULL_HANDLE;
    renderPassState.beginInfo.renderArea.offset = { 0, 0 };
    renderPassState.beginInfo.renderArea.extent.width = static_cast<uint32_t>(renderPassState.width);
    renderPassState.beginInfo.renderArea.extent.height = static_cast<uint32_t>(renderPassState.height);
    renderPassState.beginInfo.clearValueCount = 0;
    renderPassState.beginInfo.pClearValues = nullptr;
}

void Graphics::endRenderPass()
{
    renderPassState.active = false;

    vkCmdEndRenderPass(commandBuffers.at(currentFrame));

    for (auto &colorAttachment : renderPassState.renderPassConfiguration.colorAttachments)
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

    renderPassState.renderPassConfiguration.staticData.depthStencilAttachment.depthLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    renderPassState.renderPassConfiguration.staticData.depthStencilAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

    // REMOVED: Deferred upload processing (unsafe pointer usage)
    // We now allow buffer uploads to end render passes directly

	// VkImageMemoryBarrier barrier{};
	// barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	// barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	// barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	// barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	// barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	// // Fix: Use the correct image for the barrier
	// VkImage targetImage = VK_NULL_HANDLE;
	// if (!swapChainImages.empty()) {
	// 	targetImage = swapChainImages[imageIndex];
	// } else if (fakeBackbuffer) {
	// 	targetImage = reinterpret_cast<VkImage>(fakeBackbuffer->getRenderTargetHandle());
	// }
	// barrier.image = targetImage;

	// barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	// barrier.subresourceRange.baseMipLevel = 0;
	// barrier.subresourceRange.levelCount = 1;
	// barrier.subresourceRange.baseArrayLayer = 0;
	// barrier.subresourceRange.layerCount = 1;
	// barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	// barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	// vkCmdPipelineBarrier(
	// 	commandBuffers.at(currentFrame),
	// 	VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
	// 	VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
	// 	0,
	// 	0, nullptr,
	// 	0, nullptr,
	// 	1, &barrier
	// );

    // // Transition depth image to transfer src
    // if (depthImage != VK_NULL_HANDLE) {
    //     VkImageMemoryBarrier depthBarrier{};
    //     depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    //     depthBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    //     depthBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    //     depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     depthBarrier.image = depthImage;
    //     depthBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    //     depthBarrier.subresourceRange.baseMipLevel = 0;
    //     depthBarrier.subresourceRange.levelCount = 1;
    //     depthBarrier.subresourceRange.baseArrayLayer = 0;
    //     depthBarrier.subresourceRange.layerCount = 1;
    //     depthBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    //     depthBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    //     vkCmdPipelineBarrier(
    //         commandBuffers.at(currentFrame),
    //         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
    //         VK_PIPELINE_STAGE_TRANSFER_BIT,
    //         0,
    //         0, nullptr,
    //         0, nullptr,
    //         1, &depthBarrier
    //     );
    // }

    // // Create a buffer or image for the copy
    // // vkCmdCopyImage or vkCmdCopyImageToBuffer

	// if (depthImage != VK_NULL_HANDLE) {
	// 	// Declare and create a buffer for depth readback if not already present
	// 	static VkBuffer depthReadbackBuffer = VK_NULL_HANDLE;
	// 	static VmaAllocation depthReadbackBufferAllocation = VK_NULL_HANDLE;

	// 	// Calculate the size needed for the depth buffer (assuming 4 bytes per pixel for float depth)
	// 	VkDeviceSize depthBufferSize = renderPassState.framebufferConfiguration.staticData.width *
	// 								  renderPassState.framebufferConfiguration.staticData.height * sizeof(float);

	// 	if (depthReadbackBuffer == VK_NULL_HANDLE) {
	// 		VkBufferCreateInfo bufferInfo{};
	// 		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	// 		bufferInfo.size = depthBufferSize;
	// 		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	// 		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	// 		VmaAllocationCreateInfo allocInfo{};
	// 		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	// 		allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

	// 		if (vmaCreateBuffer(vmaAllocator, &bufferInfo, &allocInfo, &depthReadbackBuffer, &depthReadbackBufferAllocation, nullptr) != VK_SUCCESS) {
	// 			throw love::Exception("failed to create depth readback buffer");
	// 		}
	// 	}

	// 	VkBufferImageCopy region{};
	// 	region.bufferOffset = 0;
	// 	region.bufferRowLength = 0; // tightly packed
	// 	region.bufferImageHeight = 0;
	// 	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; // or include STENCIL if needed
	// 	region.imageSubresource.mipLevel = 0;
	// 	region.imageSubresource.baseArrayLayer = 0;
	// 	region.imageSubresource.layerCount = 1;
	// 	region.imageOffset = {0, 0, 0};
	// 	region.imageExtent = {
	// 		renderPassState.framebufferConfiguration.staticData.width,
	// 		renderPassState.framebufferConfiguration.staticData.height,
	// 		1
	// 	};

	// 	vkCmdCopyImageToBuffer(
	// 		commandBuffers.at(currentFrame),
	// 		depthImage,
	// 		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	// 		depthReadbackBuffer,
	// 		1,
	// 		&region
	// 	);
	// }

	//
	// Removed erroneous descriptor update code that referenced undefined variables.
	// If descriptor updates are needed here, implement them with valid handles and context.
	//
}

VkSampler Graphics::createSampler(const SamplerState &samplerState)
{
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = Vulkan::getFilter(samplerState.magFilter);
	samplerInfo.minFilter = Vulkan::getFilter(samplerState.minFilter);
	samplerInfo.addressModeU = Vulkan::getWrapMode(samplerState.wrapU);
	samplerInfo.addressModeV = Vulkan::getWrapMode(samplerState.wrapV);
	samplerInfo.addressModeW = Vulkan::getWrapMode(samplerState.wrapW);
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = static_cast<float>(samplerState.maxAnisotropy);

	// TODO: This probably needs to branch on a pixel format to determine whether
	// it should be float vs int, and opaque vs transparent.
	bool clampone = samplerState.wrapU == SamplerState::WRAP_CLAMP_ONE
		|| samplerState.wrapV == SamplerState::WRAP_CLAMP_ONE
		|| samplerState.wrapW == SamplerState::WRAP_CLAMP_ONE;
	samplerInfo.borderColor = clampone ? VK_BORDER_COLOR_INT_OPAQUE_WHITE : VK_BORDER_COLOR_INT_OPAQUE_BLACK;

	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	if (samplerState.depthSampleMode.hasValue)
	{
		samplerInfo.compareEnable = VK_TRUE;
		// See the comment in renderstate.h
		samplerInfo.compareOp = Vulkan::getCompareOp(getReversedCompareMode(samplerState.depthSampleMode.value));
	}
	else
	{
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	}
	samplerInfo.mipmapMode = Vulkan::getMipMapMode(samplerState.mipmapFilter);
	samplerInfo.mipLodBias = samplerState.lodBias;
	samplerInfo.minLod = static_cast<float>(samplerState.minLod);
	samplerInfo.maxLod = static_cast<float>(samplerState.maxLod);

	VkSampler sampler;
	if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
		throw love::Exception("failed to create sampler");

	return sampler;
}

void Graphics::requestSwapchainRecreation()
{
	if (swapChain != VK_NULL_HANDLE)
	{
		swapChainRecreationRequested = true;
	}
}

VkSampler Graphics::getCachedSampler(const SamplerState &samplerState)
{
	auto samplerkey = samplerState.toKey();
	auto it = samplers.find(samplerkey);
	if (it != samplers.end())
		return it->second;
	else
	{
		VkSampler sampler = createSampler(samplerState);
		samplers.insert({ samplerkey, sampler });
		return sampler;
	}
}

VkPipeline Graphics::createGraphicsPipeline(Shader *shader, const GraphicsPipelineConfigurationCore &configuration, const GraphicsPipelineConfigurationNoDynamicState *noDynamicStateConfiguration)
{
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	auto &shaderStages = shader->getShaderStages();

	std::vector<VkVertexInputBindingDescription> bindingDescriptions;
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

	VertexAttributes vertexAttributes;
    if (!findVertexAttributes(configuration.attributesID, vertexAttributes)) {
        // If ID lookup fails, use the direct attributes from configuration
        vertexAttributes = configuration.attributes;
    }

    createVulkanVertexFormat(shader, vertexAttributes, bindingDescriptions, attributeDescriptions);

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = configuration.msaaSamples;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = Vulkan::getPolygonMode(configuration.wireFrame);
	rasterizer.lineWidth = 1.0f;
	if (!optionalDeviceExtensions.extendedDynamicState)
	{
		rasterizer.cullMode = Vulkan::getCullMode(noDynamicStateConfiguration->cullmode);
		rasterizer.frontFace = Vulkan::getFrontFace(noDynamicStateConfiguration->winding);
	}

	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = Vulkan::getPrimitiveTypeTopology(configuration.primitiveType);
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	if (!optionalDeviceExtensions.extendedDynamicState)
	{
		// depthStencil.depthWriteEnable = Vulkan::getBool(noDynamicStateConfiguration->depthState.write);
		// depthStencil.depthCompareOp = Vulkan::getCompareOp(noDynamicStateConfiguration->depthState.compare);
        depthStencil.depthWriteEnable = configuration.depthWriteEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	}
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f;
	depthStencil.maxDepthBounds = 1.0f;

	depthStencil.stencilTestEnable = VK_TRUE;

	if (!optionalDeviceExtensions.extendedDynamicState)
	{
		depthStencil.front.failOp = VK_STENCIL_OP_KEEP;
		depthStencil.front.passOp = Vulkan::getStencilOp(noDynamicStateConfiguration->stencilAction);
		depthStencil.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depthStencil.front.compareOp = Vulkan::getCompareOp(getReversedCompareMode(noDynamicStateConfiguration->stencilCompare));

		depthStencil.back.failOp = VK_STENCIL_OP_KEEP;
		depthStencil.back.passOp = Vulkan::getStencilOp(noDynamicStateConfiguration->stencilAction);
		depthStencil.back.depthFailOp = VK_STENCIL_OP_KEEP;
		depthStencil.back.compareOp = Vulkan::getCompareOp(getReversedCompareMode(noDynamicStateConfiguration->stencilCompare));
	}

	pipelineInfo.pDepthStencilState = &depthStencil;

	BlendState blendState = BlendState::fromKey(configuration.blendStateKey);

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = Vulkan::getColorMask(configuration.colorChannelMask);
    
    // CRITICAL DEBUG: Check color write mask
    // std::printf("[CHAILOVE DEBUG] Pipeline color configuration:\n");
    // std::printf("[CHAILOVE DEBUG] - configuration.colorChannelMask: 0x%X\n", configuration.colorChannelMask);
    // std::printf("[CHAILOVE DEBUG] - colorWriteMask (Vulkan): 0x%X\n", colorBlendAttachment.colorWriteMask);
    // std::printf("[CHAILOVE DEBUG] - numColorAttachments: %u\n", configuration.numColorAttachments);
    
    if (colorBlendAttachment.colorWriteMask == 0) {
        // std::printf("[CHAILOVE ERROR] COLOR WRITE MASK IS ZERO - THIS CAUSES DEPTH-ONLY RENDERING!\n");
        // Force enable all color channels for debugging
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        // std::printf("[CHAILOVE DEBUG] Forced colorWriteMask to: 0x%X\n", colorBlendAttachment.colorWriteMask);
    }
	colorBlendAttachment.blendEnable = Vulkan::getBool(blendState.enable);
	colorBlendAttachment.srcColorBlendFactor = Vulkan::getBlendFactor(blendState.srcFactorRGB);
	colorBlendAttachment.dstColorBlendFactor = Vulkan::getBlendFactor(blendState.dstFactorRGB);
	colorBlendAttachment.colorBlendOp = Vulkan::getBlendOp(blendState.operationRGB);
	colorBlendAttachment.srcAlphaBlendFactor = Vulkan::getBlendFactor(blendState.srcFactorA);
	colorBlendAttachment.dstAlphaBlendFactor = Vulkan::getBlendFactor(blendState.dstFactorA);
	colorBlendAttachment.alphaBlendOp = Vulkan::getBlendOp(blendState.operationA);

	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(configuration.numColorAttachments, colorBlendAttachment);

	if (blendState.enable)
	{
		for (uint32 i = 0; i < configuration.numColorAttachments; i++)
		{
			PixelFormat format = (PixelFormat)((configuration.packedColorAttachmentFormats >> (i * 8ull)) & 0xFF);
			if (!isPixelFormatSupported(format, PIXELFORMATUSAGEFLAGS_BLEND))
				colorBlendAttachments[i].blendEnable = false;
		}
	}

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
	colorBlending.pAttachments = colorBlendAttachments.data();
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	std::vector<VkDynamicState> dynamicStates;

	if (optionalDeviceExtensions.extendedDynamicState)
		dynamicStates = {
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
			VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
			VK_DYNAMIC_STATE_STENCIL_REFERENCE,

			VK_DYNAMIC_STATE_CULL_MODE_EXT,
			VK_DYNAMIC_STATE_FRONT_FACE_EXT,
			VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT,
			VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT,
			VK_DYNAMIC_STATE_STENCIL_OP_EXT,
		};
	else
		dynamicStates = {
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
			VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
			VK_DYNAMIC_STATE_STENCIL_REFERENCE,
		};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = shader->getGraphicsPipelineLayout();
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;
	pipelineInfo.renderPass = configuration.renderPass;

	VkPipeline graphicsPipeline;
	if (vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
		throw love::Exception("failed to create graphics pipeline");
	return graphicsPipeline;
}

VkSampleCountFlagBits Graphics::getMsaaCount(int requestedMsaa) const
{
	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

	VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;

	if (counts & VK_SAMPLE_COUNT_64_BIT && requestedMsaa >= 64)
		return VK_SAMPLE_COUNT_64_BIT;
	else if (counts & VK_SAMPLE_COUNT_32_BIT && requestedMsaa >= 32)
		return VK_SAMPLE_COUNT_32_BIT;
	else if (counts & VK_SAMPLE_COUNT_16_BIT && requestedMsaa >= 16)
		return VK_SAMPLE_COUNT_16_BIT;
	else if (counts & VK_SAMPLE_COUNT_8_BIT && requestedMsaa >= 8)
		return VK_SAMPLE_COUNT_8_BIT;
	else if (counts & VK_SAMPLE_COUNT_4_BIT && requestedMsaa >= 4)
		return VK_SAMPLE_COUNT_4_BIT;
	else if (counts & VK_SAMPLE_COUNT_2_BIT && requestedMsaa >= 2)
		return VK_SAMPLE_COUNT_2_BIT;
	else
		return VK_SAMPLE_COUNT_1_BIT;
}

void Graphics::setVsync(int vsync)
{
	if (vsync != this->vsync)
	{
		this->vsync = vsync;

		// With the extension VK_EXT_swapchain_maintenance1 a swapchain recreation might not be needed
		// https://github.com/KhronosGroup/Vulkan-Docs/blob/main/proposals/VK_EXT_swapchain_maintenance1.adoc
		// However, there are not any drivers that support it, yet.
		// Reevaluate again in the future.

		requestSwapchainRecreation();
	}
}

int Graphics::getVsync() const
{
	return vsync;
}

void Graphics::mapLocalUniformData(void *data, size_t size, VkDescriptorBufferInfo &bufferInfo)
{
	size_t alignedSize = alignUp(size, minUniformBufferOffsetAlignment);

	if (localUniformBuffer->getUsableSize() < alignedSize)
		localUniformBuffer.set(new StreamBuffer(this, BUFFERUSAGE_UNIFORM, localUniformBuffer->getSize() * 2), Acquire::NORETAIN);

	auto mapInfo = localUniformBuffer->map(size);
	memcpy(mapInfo.data, data, size);

	bufferInfo.buffer = (VkBuffer)localUniformBuffer->getHandle();
	bufferInfo.offset = localUniformBuffer->unmap(size);
	bufferInfo.range = size;

	localUniformBuffer->markUsed(alignedSize);
}

void Graphics::createColorResources()
{
	if (msaaSamples & VK_SAMPLE_COUNT_1_BIT)
	{
		colorImage = VK_NULL_HANDLE;
		colorImageView = VK_NULL_HANDLE;
	} 
	else
	{
		VkFormat colorFormat = swapChainImageFormat;

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = colorFormat;
		imageInfo.extent.width = swapChainExtent.width;
		imageInfo.extent.height = swapChainExtent.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = msaaSamples;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VmaAllocationCreateInfo allocationInfo{};
		allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocationInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

		if (vmaCreateImage(vmaAllocator, &imageInfo, &allocationInfo, &colorImage, &colorImageAllocation, nullptr))
			throw love::Exception("failed to create color image");

		VkImageViewCreateInfo imageViewInfo{};
		imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewInfo.image = colorImage;
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = colorFormat;
		imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device, &imageViewInfo, nullptr, &colorImageView) != VK_SUCCESS)
			throw love::Exception("failed to create color image view");
	}
}

VkFormat Graphics::findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (auto format : candidates)
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
		if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features)
			return format;
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features)
			return format;
	}

	throw love::Exception("failed to find supported format");
}

VkFormat Graphics::findDepthFormat()
{
	return findSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

void Graphics::createDepthResources()
{
    if (!backbufferHasDepth && !backbufferHasStencil)
    {
        depthImage = VK_NULL_HANDLE;
        depthImageView = VK_NULL_HANDLE;
        return;
    }

    // CRITICAL FIX: Validate prerequisites before attempting to create resources
    if (device == VK_NULL_HANDLE) {
        throw love::Exception("createDepthResources: Vulkan device not initialized");
    }
    
    if (vmaAllocator == VK_NULL_HANDLE) {
        throw love::Exception("createDepthResources: VMA allocator not initialized");
    }
    
    if (depthStencilFormat == VK_FORMAT_UNDEFINED) {
        throw love::Exception("createDepthResources: depthStencilFormat is undefined");
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = depthStencilFormat;

    // In libretro mode, use window dimensions instead of swapchain extent
    // Ensure dimensions are at least 1x1 to prevent Vulkan validation errors
    if (!swapChainImages.empty()) {
        imageInfo.extent.width = swapChainExtent.width;
        imageInfo.extent.height = swapChainExtent.height;
    } else {
        // Use window dimensions for libretro mode, ensure minimum of 1x1
        imageInfo.extent.width = static_cast<uint32_t>(std::max(1, pixelWidth));
        imageInfo.extent.height = static_cast<uint32_t>(std::max(1, pixelHeight));
    }

    // Additional validation for image dimensions
    if (imageInfo.extent.width == 0 || imageInfo.extent.height == 0) {
        throw love::Exception("createDepthResources: Invalid image dimensions (%u x %u)", 
                             imageInfo.extent.width, imageInfo.extent.height);
    }

    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = msaaSamples;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VkResult imageResult = vmaCreateImage(vmaAllocator, &imageInfo, &allocationInfo, &depthImage, &depthImageAllocation, nullptr);
    if (imageResult != VK_SUCCESS) {
        throw love::Exception("failed to create depth image: VkResult = %d", imageResult);
    }

    VkImageViewCreateInfo imageViewInfo{};
    imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.image = depthImage;
    imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewInfo.format = depthStencilFormat;
    imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    
    imageViewInfo.subresourceRange.aspectMask = 0;
    if (backbufferHasDepth)
        imageViewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
    if (backbufferHasStencil)
        imageViewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    
    imageViewInfo.subresourceRange.baseMipLevel = 0;
    imageViewInfo.subresourceRange.levelCount = 1;
    imageViewInfo.subresourceRange.baseArrayLayer = 0;
    imageViewInfo.subresourceRange.layerCount = 1;

    VkResult viewResult = vkCreateImageView(device, &imageViewInfo, nullptr, &depthImageView);
    if (viewResult != VK_SUCCESS) {
        // Clean up the image if image view creation fails
        vmaDestroyImage(vmaAllocator, depthImage, depthImageAllocation);
        throw love::Exception("failed to create depth image view: VkResult = %d", viewResult);
    }
}

void Graphics::createCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
		throw love::Exception("failed to create command pool");
}

void Graphics::createCommandBuffers()
{
    if (libretroMode && commandPool == VK_NULL_HANDLE) {
        throw love::Exception("Command pool is null in libretro mode - cannot create command buffers");
    }
    
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
        throw love::Exception("failed to allocate command buffers");
}

void Graphics::createSyncObjects()
{
	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
	imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores.at(i)) != VK_SUCCESS ||
			vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores.at(i)) != VK_SUCCESS ||
			vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences.at(i)) != VK_SUCCESS)
			throw love::Exception("failed to create synchronization objects for a frame!");
}

void Graphics::cleanup()
{
	for (auto &cleanUpFns : cleanUpFunctions)
		for (auto &cleanUpFn : cleanUpFns)
			cleanUpFn();
	cleanUpFunctions.clear();

	vmaDestroyAllocator(vmaAllocator);

	for (const auto &s : renderFinishedSemaphores)
		vkDestroySemaphore(device, s, nullptr);
	renderFinishedSemaphores.clear();

	for (const auto &s : imageAvailableSemaphores)
		vkDestroySemaphore(device, s, nullptr);
	imageAvailableSemaphores.clear();

	for (const auto &f : inFlightFences)
		vkDestroyFence(device, f, nullptr);
	inFlightFences.clear();

	if (!commandBuffers.empty())
		vkFreeCommandBuffers(device, commandPool, (uint32)commandBuffers.size(), commandBuffers.data());
	commandBuffers.clear();

	for (auto const &p : samplers)
		vkDestroySampler(device, p.second, nullptr);
	samplers.clear();

	for (const auto &entry : renderPasses)
		vkDestroyRenderPass(device, entry.second, nullptr);
	renderPasses.clear();

	for (const auto &entry : framebuffers)
		vkDestroyFramebuffer(device, entry.second, nullptr);
	framebuffers.clear();

	vkDestroyCommandPool(device, commandPool, nullptr);
	vkDestroyPipelineCache(device, pipelineCache, nullptr);
	vkDestroyDevice(device, nullptr);
}

void Graphics::cleanupSwapChain()
{
	if (colorImage)
	{
		cleanupFramebuffers(colorImageView, swapChainPixelFormat);
		vkDestroyImageView(device, colorImageView, nullptr);
		vmaDestroyImage(vmaAllocator, colorImage, colorImageAllocation);
	}
	if (depthImage)
	{
		cleanupFramebuffers(depthImageView, depthStencilPixelFormat);
		vkDestroyImageView(device, depthImageView, nullptr);
		vmaDestroyImage(vmaAllocator, depthImage, depthImageAllocation);
	}
	for (const auto &swapChainImageView : swapChainImageViews)
	{
		cleanupFramebuffers(swapChainImageView, swapChainPixelFormat);
		vkDestroyImageView(device, swapChainImageView, nullptr);
	}
	swapChainImageViews.clear();
	vkDestroySwapchainKHR(device, swapChain, nullptr);
	swapChainImages.clear();
	fakeBackbuffer.set(nullptr);

	swapChain = VK_NULL_HANDLE;
}

void Graphics::recreateSwapChain()
{
	vkDeviceWaitIdle(device);

	cleanupSwapChain();

	createSwapChain();
	createImageViews();
	createColorResources();
	createDepthResources();

	transitionColorDepthLayouts = true;
}

love::gfx::Graphics *createInstance()
{
	love::gfx::Graphics *instance = nullptr;

	try
	{
		instance = new Graphics();
	}
	catch (love::Exception &e)
	{
		printf("Cannot create Vulkan renderer: %s\n", e.what());
	}

	return instance;
}

void Graphics::deferBufferUpload(Buffer* buffer, size_t offset, size_t size, const void* data)
{
    // Store the upload for processing when render pass ends
    deferredUploads.emplace_back(buffer, offset, size, data);
    
    // Debug output
    // std::printf("[LIBRETRO] Deferring buffer upload: buffer=%p, offset=%zu, size=%zu\n", 
    //        buffer, offset, size);
}

} // vulkan
} // graphics
} // love

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

#include "Buffer.h"
#include "Graphics.h"

namespace love
{
namespace gfx
{
namespace vulkan
{

static VkBufferUsageFlags getUsageBit(BufferUsage mode)
{
	switch (mode)
	{
	case BUFFERUSAGE_VERTEX: return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	case BUFFERUSAGE_INDEX: return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	case BUFFERUSAGE_UNIFORM: return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	case BUFFERUSAGE_TEXEL: return VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
	case BUFFERUSAGE_SHADER_STORAGE: return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	case BUFFERUSAGE_INDIRECT_ARGUMENTS: return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	default:
		throw love::Exception("unsupported BufferUsage mode");
	}
}

static VkBufferUsageFlags getVulkanUsageFlags(BufferUsageFlags flags)
{
	VkBufferUsageFlags vkFlags = 0;
	for (int i = 0; i < BUFFERUSAGE_MAX_ENUM; i++)
	{
		BufferUsageFlags flag = static_cast<BufferUsageFlags>(1u << i);
		if (flags & flag)
			vkFlags |= getUsageBit((BufferUsage)i);
	}
	return vkFlags;
}

Buffer::Buffer(love::gfx::Graphics *gfx, const Settings &settings, const std::vector<DataDeclaration> &format, const void *data, size_t size, size_t arraylength)
	: love::gfx::Buffer(gfx, settings, format, size, arraylength)
	, zeroInitialize(settings.zeroInitialize)
	, initialData(data)
	, vgfx(dynamic_cast<Graphics*>(gfx))
	, usageFlags(settings.usageFlags)
{
	// All buffers can be copied to and from.
	barrierDstAccessFlags = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	barrierDstStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;

	if (usageFlags & BUFFERUSAGEFLAG_VERTEX)
	{
		barrierDstAccessFlags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		barrierDstStageFlags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
	}
	if (usageFlags & BUFFERUSAGEFLAG_INDEX)
	{
		barrierDstAccessFlags |= VK_ACCESS_INDEX_READ_BIT;
		barrierDstStageFlags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
	}
	if (usageFlags & BUFFERUSAGEFLAG_TEXEL)
	{
		barrierDstAccessFlags |= VK_ACCESS_SHADER_READ_BIT;
		barrierDstStageFlags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	}
	if (usageFlags & BUFFERUSAGEFLAG_SHADER_STORAGE)
	{
		barrierDstAccessFlags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		barrierDstStageFlags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	}
	if (usageFlags & BUFFERUSAGEFLAG_INDIRECT_ARGUMENTS)
	{
		barrierDstAccessFlags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		barrierDstStageFlags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
	}

	loadVolatile();
}

bool Buffer::loadVolatile()
{
	allocator = vgfx->getVmaAllocator();

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = getSize();
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | getVulkanUsageFlags(usageFlags);

	VmaAllocationCreateInfo allocCreateInfo{};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
	if (dataUsage == BUFFERDATAUSAGE_READBACK)
		allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

	auto result = vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &buffer, &allocation, &allocInfo);
	if (result != VK_SUCCESS)
		throw love::Exception("failed to create buffer");

	if (zeroInitialize)
	{
		auto cmd = vgfx->getCommandBufferForDataTransfer();
		vkCmdFillBuffer(cmd, buffer, 0, VK_WHOLE_SIZE, 0);
		postGPUWriteBarrier(cmd);
	}

	if (initialData)
		fill(0, size, initialData);

	if (usageFlags & BUFFERUSAGEFLAG_TEXEL)
	{
		VkBufferViewCreateInfo bufferViewInfo{};
		bufferViewInfo.buffer = buffer;
		bufferViewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
		bufferViewInfo.format = Vulkan::getVulkanVertexFormat(getDataMember(0).decl.format);
		bufferViewInfo.range = VK_WHOLE_SIZE;

		if (vkCreateBufferView(vgfx->getDevice(), &bufferViewInfo, nullptr, &bufferView) != VK_SUCCESS)
			throw love::Exception("failed to create texel buffer view");
	}

	VkMemoryPropertyFlags memoryProperties;
	vmaGetAllocationMemoryProperties(allocator, allocation, &memoryProperties);
	if (memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
		coherent = true;
	else
		coherent = false;

	if (!debugName.empty() && vgfx->getEnabledOptionalInstanceExtensions().debugInfo)
	{
		auto device = vgfx->getDevice();

		VkDebugUtilsObjectNameInfoEXT nameInfo{};
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		nameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
		nameInfo.objectHandle = (uint64_t)buffer;
		nameInfo.pObjectName = debugName.c_str();
		vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
	}

	return true;
}

void Buffer::unloadVolatile()
{
	if (buffer == VK_NULL_HANDLE)
		return;

	auto device = vgfx->getDevice();

	vgfx->queueCleanUp(
		[device=device, allocator=allocator, buffer=buffer, allocation=allocation, bufferView=bufferView](){
		vkDeviceWaitIdle(device);
		vmaDestroyBuffer(allocator, buffer, allocation);
		if (bufferView)
			vkDestroyBufferView(device, bufferView, nullptr);
	});

	buffer = VK_NULL_HANDLE;
	bufferView = VK_NULL_HANDLE;
}

Buffer::~Buffer()
{
	unloadVolatile();
}

ptrdiff_t Buffer::getHandle() const
{
	return (ptrdiff_t) buffer;
}

ptrdiff_t Buffer::getTexelBufferHandle() const
{
	return (ptrdiff_t) bufferView;
}

void *Buffer::map(MapType map, size_t offset, size_t size)
{
	if (size == 0)
		return nullptr;

	if (map == MAP_WRITE_INVALIDATE && (isImmutable() || dataUsage == BUFFERDATAUSAGE_READBACK))
		return nullptr;

	if (map == MAP_READ_ONLY && dataUsage != BUFFERDATAUSAGE_READBACK)
		return  nullptr;

	mappedRange = Range(offset, size);

	if (!Range(0, getSize()).contains(mappedRange))
		return nullptr;

	if (dataUsage == BUFFERDATAUSAGE_READBACK)
	{
		if (!coherent)
			vmaInvalidateAllocation(allocator, allocation, offset, size);

		char *data = (char*)allocInfo.pMappedData;
		return (void*) (data + offset);
	}
	else
	{
		// Use staging buffer pool to avoid constant allocation/deallocation
		auto stagingBuf = vgfx->acquireStagingBuffer(size);
		if (!stagingBuf)
			throw love::Exception("failed to acquire staging buffer from pool");
		
		// Store the staging buffer info for unmap
		stagingBuffer = stagingBuf->buffer;
		stagingAllocation = stagingBuf->allocation;
		stagingAllocInfo = stagingBuf->allocInfo;

		return stagingAllocInfo.pMappedData;
	}
}

bool Buffer::fill(size_t offset, size_t size, const void *data)
{
    if (size == 0 || isImmutable() || dataUsage == BUFFERDATAUSAGE_READBACK)
        return false;

    if (!Range(0, getSize()).contains(Range(offset, size)))
        return false;

    // std::printf("[LIBRETRO] Buffer upload START: buffer=%p, offset=%zu, size=%zu\n", this, offset, size);
    
    // Validate inputs
    if (!allocator || !data) {
        // std::printf("[LIBRETRO] ERROR: Invalid allocator or data\n");
        return false;
    }

    // Use staging buffer pool from Graphics instead of creating temporary buffer
    auto stagingBuf = vgfx->acquireStagingBuffer(size);
    if (!stagingBuf)
        return false;

    // Copy data to staging buffer
    memcpy(stagingBuf->allocInfo.pMappedData, data, size);

    VkMemoryPropertyFlags memoryProperties;
    vmaGetAllocationMemoryProperties(allocator, stagingBuf->allocation, &memoryProperties);
    if (~memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        vmaFlushAllocation(allocator, stagingBuf->allocation, 0, size);

    // LIBRETRO FIX: Create separate command buffer for immediate transfer
    VkCommandBuffer transferCmd = VK_NULL_HANDLE;
    VkCommandPool commandPool = vgfx->getCommandPool();
    
    VkCommandBufferAllocateInfo allocInfoCmd{};
    allocInfoCmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfoCmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfoCmd.commandPool = commandPool;
    allocInfoCmd.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(vgfx->getDevice(), &allocInfoCmd, &transferCmd) != VK_SUCCESS) {
        // std::printf("[LIBRETRO] ERROR: Failed to allocate transfer command buffer\n");
        vgfx->releaseStagingBuffer(stagingBuf);
        return false;
    }

    // Begin transfer command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(transferCmd, &beginInfo) != VK_SUCCESS) {
        // std::printf("[LIBRETRO] ERROR: Failed to begin transfer command buffer\n");
        vkFreeCommandBuffers(vgfx->getDevice(), commandPool, 1, &transferCmd);
        vgfx->releaseStagingBuffer(stagingBuf);
        return false;
    }

    // Record transfer commands
    VkBufferCopy bufferCopy{};
    bufferCopy.srcOffset = 0;
    bufferCopy.dstOffset = offset;
    bufferCopy.size = size;

    vkCmdCopyBuffer(transferCmd, stagingBuf->buffer, buffer, 1, &bufferCopy);
    
    // Add memory barrier
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = barrierDstAccessFlags;

    vkCmdPipelineBarrier(transferCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, barrierDstStageFlags, 0, 1, &barrier, 0, nullptr, 0, nullptr);

    // End command buffer
    if (vkEndCommandBuffer(transferCmd) != VK_SUCCESS) {
        // std::printf("[LIBRETRO] ERROR: Failed to end transfer command buffer\n");
        vkFreeCommandBuffers(vgfx->getDevice(), commandPool, 1, &transferCmd);
        vgfx->releaseStagingBuffer(stagingBuf);
        return false;
    }

    // Submit immediately and wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &transferCmd;

    // Get the queue (assumes we have access to it in libretro mode)
    VkQueue queue = vgfx->getQueue(); // We'll need to add this method
    
    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        // std::printf("[LIBRETRO] ERROR: Failed to submit transfer commands\n");
        vkFreeCommandBuffers(vgfx->getDevice(), commandPool, 1, &transferCmd);
        vgfx->releaseStagingBuffer(stagingBuf);
        return false;
    }

    // Wait for completion
    vkQueueWaitIdle(queue);
    
    // std::printf("[LIBRETRO] Transfer completed and synchronized\n");

    // Cleanup
    vkFreeCommandBuffers(vgfx->getDevice(), commandPool, 1, &transferCmd);
    vgfx->releaseStagingBuffer(stagingBuf);

    return true;
}

// Add this method after the fill() method:

bool Buffer::fillImmediate(size_t offset, size_t size, const void *data)
{
    if (size == 0 || isImmutable() || dataUsage == BUFFERDATAUSAGE_READBACK)
        return false;

    if (!Range(0, getSize()).contains(Range(offset, size)))
        return false;

    // Direct upload without checking render pass state
    // Use staging buffer pool instead of creating temporary buffer
    auto stagingBuf = vgfx->acquireStagingBuffer(size);
    if (!stagingBuf)
        throw love::Exception("failed to acquire staging buffer");

    memcpy(stagingBuf->allocInfo.pMappedData, data, size);

    VkMemoryPropertyFlags memoryProperties;
    vmaGetAllocationMemoryProperties(allocator, stagingBuf->allocation, &memoryProperties);
    if (~memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        vmaFlushAllocation(allocator, stagingBuf->allocation, 0, size);

    VkBufferCopy bufferCopy{};
    bufferCopy.srcOffset = 0;
    bufferCopy.dstOffset = offset;
    bufferCopy.size = size;

    auto cmd = vgfx->getCommandBufferForDataTransfer();
    vkCmdCopyBuffer(cmd, stagingBuf->buffer, buffer, 1, &bufferCopy);

    postGPUWriteBarrier(cmd);

    // Queue cleanup to release staging buffer after GPU is done
    vgfx->queueCleanUp([vgfx = vgfx, stagingBuf]() mutable {
        vgfx->releaseStagingBuffer(stagingBuf);
    });

    return true;
}

void Buffer::unmap(size_t usedoffset, size_t usedsize)
{
	if (dataUsage != BUFFERDATAUSAGE_READBACK)
	{
		VkBufferCopy bufferCopy{};
		bufferCopy.srcOffset = usedoffset - mappedRange.getOffset();
		bufferCopy.dstOffset = usedoffset;
		bufferCopy.size = usedsize;

		VkMemoryPropertyFlags memoryProperties;
		vmaGetAllocationMemoryProperties(allocator, stagingAllocation, &memoryProperties);
		if (~memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
			vmaFlushAllocation(allocator, stagingAllocation, bufferCopy.srcOffset, usedsize);

		auto cmd = vgfx->getCommandBufferForDataTransfer();
		vkCmdCopyBuffer(cmd, stagingBuffer, buffer, 1, &bufferCopy);

		postGPUWriteBarrier(cmd);

		// Return staging buffer to pool instead of destroying it
		vgfx->queueCleanUp([vgfx = vgfx, stagingBuffer = stagingBuffer]() {
			vgfx->releaseStagingBuffer(stagingBuffer);
		});
	}
}

void Buffer::clearInternal(size_t offset, size_t size)
{
	auto cmd = vgfx->getCommandBufferForDataTransfer();
	vkCmdFillBuffer(cmd, buffer, offset, size, 0);
	postGPUWriteBarrier(cmd);
}

void Buffer::copyTo(love::gfx::Buffer *dest, size_t sourceoffset, size_t destoffset, size_t size)
{
	auto commandBuffer = vgfx->getCommandBufferForDataTransfer();

	VkBufferCopy bufferCopy{};
	bufferCopy.srcOffset = sourceoffset;
	bufferCopy.dstOffset = destoffset;
	bufferCopy.size = size;

	vkCmdCopyBuffer(commandBuffer, buffer, (VkBuffer) dest->getHandle(), 1, &bufferCopy);

	((Buffer *)dest)->postGPUWriteBarrier(commandBuffer);
}

void Buffer::postGPUWriteBarrier(VkCommandBuffer cmd)
{
	VkMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = barrierDstAccessFlags;

	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, barrierDstStageFlags, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

} // vulkan
} // graphics
} // love

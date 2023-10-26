#pragma once

#include "Device.hpp"

class WrpBuffer
{
public:
    WrpBuffer(
        WrpDevice& device,
        VkDeviceSize instanceSize,
        uint32_t instanceCount,
        VkBufferUsageFlags usageFlags,
        VkMemoryPropertyFlags memoryPropertyFlags,
        VkDeviceSize minOffsetAlignment = 1);
    ~WrpBuffer();

    WrpBuffer(const WrpBuffer&) = delete;
    WrpBuffer& operator=(const WrpBuffer&) = delete;

    VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void unmap();

    void writeToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    void writeToIndex(void* data, int index);
    VkResult flushIndex(int index);
    VkDescriptorBufferInfo descriptorInfoForIndex(int index);
    VkResult invalidateIndex(int index);

    VkBuffer getBuffer() const { return buffer; }
    void* getMappedMemory() const { return mapped; }
    uint32_t getInstanceCount() const { return instanceCount; }
    VkDeviceSize getInstanceSize() const { return instanceSize; }
    VkDeviceSize getAlignmentSize() const { return instanceSize; }
    VkBufferUsageFlags getUsageFlags() const { return usageFlags; }
    VkMemoryPropertyFlags getMemoryPropertyFlags() const { return memoryPropertyFlags; }
    VkDeviceSize getBufferSize() const { return bufferSize; }

private:
    static VkDeviceSize getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);

    WrpDevice& wrpDevice;
    void* mapped = nullptr;
    VkBuffer buffer = VK_NULL_HANDLE;			// В Vulkan буфер и присвоенная ему память - два отдельных объекта.
    VkDeviceMemory memory = VK_NULL_HANDLE;		// Это позволяет получить полный контроль над управлением памятью.

    VkDeviceSize bufferSize;
    uint32_t instanceCount;
    VkDeviceSize instanceSize;
    VkDeviceSize alignmentSize;
    VkBufferUsageFlags usageFlags;
    VkMemoryPropertyFlags memoryPropertyFlags;
};

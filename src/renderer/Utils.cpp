#include "Utils.hpp"

VkResult createSemaphore(VkDevice device, VkSemaphore* outSemaphore)
{
    VkSemaphoreCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    return vkCreateSemaphore(device, &createInfo, nullptr, outSemaphore);
} 

#include "Utils.hpp"

#include <chrono>
#include <ctime>

VkResult createSemaphore(VkDevice device, VkSemaphore* outSemaphore)
{
    VkSemaphoreCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    return vkCreateSemaphore(device, &createInfo, nullptr, outSemaphore);
} 

std::string getTimeStampStr()
{
    auto timePoint = std::chrono::system_clock::now();
    std::time_t timeStamp = std::chrono::system_clock::to_time_t(timePoint);
    return std::string(std::ctime(&timeStamp));
}

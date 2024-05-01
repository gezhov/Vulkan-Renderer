#pragma once

#include "HeaderCore.hpp"
#include "Window.hpp"

#include <string>
#include <vector>
#include <optional>

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

class WrpDevice
{
public:
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

    WrpDevice(WrpWindow& window);
    ~WrpDevice();

    // Not copyable or movable
    WrpDevice(const WrpDevice&) = delete;
    WrpDevice& operator=(const WrpDevice&) = delete;
    WrpDevice(WrpDevice&&) = delete;
    WrpDevice& operator=(WrpDevice&&) = delete;

    VkCommandPool getCommandPool() { return commandPool; }
    VkDevice device() { return device_; }
    VkSurfaceKHR surface() { return surface_; }
    VkQueue graphicsQueue() { return graphicsQueue_; }
    VkQueue presentQueue() { return presentQueue_; }
    VkInstance getInstance() { return instance; }
    VkPhysicalDevice getPhysicalDevice() { return physicalDevice_; }
    uint32_t getGraphicsQueueFamily() { return getQueueFamilies().graphicsFamily.value(); }

    SwapChainSupportDetails getSwapChainSupport() { return querySwapChainSupportDetails(physicalDevice_); }
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    QueueFamilyIndices getQueueFamilies() { return findQueueFamilies(physicalDevice_); }
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkSampleCountFlagBits getMaxUsableMSAASampleCount();

    // Buffer Helper Functions
    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount);

    void createImageWithInfo(
        const VkImageCreateInfo& imageInfo,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory);

    VkPhysicalDeviceProperties properties;

private:
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();

    bool isDeviceSuitable(VkPhysicalDevice device);
    std::vector<const char*> getRequiredInstanceExtensions();
    bool checkValidationLayerSupport();
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    void populateDebugReportCallbackInfo(VkDebugReportCallbackCreateInfoEXT& createInfo);
    void checkRequiredInstanceExtensionsAvailability();
    bool checkDeviceExtensionsSupport(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupportDetails(VkPhysicalDevice device);

    WrpWindow& window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkDebugReportCallbackEXT debugReportCallback;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkCommandPool commandPool;

    VkDevice device_;
    VkSurfaceKHR surface_;
    VkQueue graphicsQueue_;
    VkQueue presentQueue_;

    const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};
    const std::vector<const char*> instanceExtensions = {VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME};
    const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
};

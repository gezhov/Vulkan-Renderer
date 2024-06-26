#pragma once

#include "Device.hpp"

#include <memory>
#include <string>
#include <vector>

class WrpSwapChain
{
public:
    WrpSwapChain(WrpDevice& device, WrpWindow& window);
    WrpSwapChain(WrpDevice& device, WrpWindow& window, std::shared_ptr<WrpSwapChain> previous);
    ~WrpSwapChain();

    WrpSwapChain(const WrpSwapChain&) = delete;
    WrpSwapChain& operator=(const WrpSwapChain&) = delete;

    VkFramebuffer getFrameBuffer(int index) { return swapChainFramebuffers[index]; }
    VkRenderPass getRenderPass() { return renderPass; }
    VkImageView getImageView(int index) { return swapChainImageViews[index]; }
    size_t getImageCount() { return imageCount; }
    VkFormat getSwapChainImageFormat() { return swapChainImageFormat; }
    VkExtent2D getSwapChainExtent() { return swapChainExtent; }
    uint32_t width() { return swapChainExtent.width; }
    uint32_t height() { return swapChainExtent.height; }

    float extentAspectRatio()
    {
        return static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);
    }
    VkFormat findDepthFormat();

    VkResult acquireNextImage(uint32_t* imageIndex);
    VkResult submitCommandBuffers(const VkCommandBuffer* buffers, uint32_t* imageIndex);

    bool compareSwapChainFormats(const WrpSwapChain& swapChain) const
    {
        return swapChain.swapChainDepthFormat == swapChainDepthFormat && swapChain.swapChainImageFormat == swapChainImageFormat;
    }

private:
    void init();
    void createSwapChain();
    void createImageViews();
    void createColorResources();
    void createDepthResources();
    void createRenderPass();
    void createFramebuffers();
    void createSyncObjects();

    // Helper functions that looks for necessary details for swap chain creation
    VkSurfaceFormatKHR chooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapChainPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapChainExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    VkFormat swapChainImageFormat;
    VkFormat swapChainDepthFormat;
    VkExtent2D swapChainExtent;

    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkRenderPass renderPass;

    // color buffer used for multisampling
    VkImage colorImage;
    VkDeviceMemory colorImageMemory;
    VkImageView colorImageView;
    VkSampleCountFlagBits msaaSampleCount;

    std::vector<VkImage> depthImages;
    std::vector<VkDeviceMemory> depthImageMemories;
    std::vector<VkImageView> depthImageViews;

    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;

    WrpDevice& wrpDevice;
    WrpWindow& wrpWindow;

    VkSwapchainKHR swapChain;
    std::shared_ptr<WrpSwapChain> oldSwapChain;

    uint32_t imageCount = 0; // also defines count of frame buffers and command buffers
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences; // fences to control command buffer recording (only after successful execution in the queue)
    std::vector<VkFence> imagesInFlight;
    size_t currentFrame = 0;
};

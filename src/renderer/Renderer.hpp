#pragma once

#include "Window.hpp"
#include "SwapChain.hpp"
#include "Device.hpp"

// libs
#include <imgui.h>

// std
#include <cassert>
#include <memory>
#include <vector>

class WrpRenderer
{
public:
    WrpRenderer(WrpWindow& window, WrpDevice& device);
    ~WrpRenderer();

    // Избавляемся от copy operator и copy constructor, т.к. WrpRenderer хранит в себе указатели
    // на VkCommandBuffer_T, которые лучше не копировать.
    WrpRenderer(const WrpRenderer&) = delete;
    WrpRenderer& operator=(const WrpRenderer&) = delete;

    VkRenderPass getSwapChainRenderPass() const { return wrpSwapChain->getRenderPass(); }
    float getAspectRatio() const {return wrpSwapChain->extentAspectRatio();}
    bool isFrameInProgress() const { return isFrameStarted; }

    VkCommandBuffer getCurrentCommandBuffer() const
    {
        assert(isFrameStarted && "Cannot get command buffer when frame not in progress");
        return commandBuffers[currentFrameIndex];
    }

    int getFrameIndex() const
    {
        assert(isFrameStarted && "Cannot get frame index when frame not in progress");
        return currentFrameIndex;
    }

    VkCommandBuffer beginFrame();
    void endFrame();
    void beginSwapChainRenderPass(VkCommandBuffer commandBuffer, ImVec4 clearColors);
    void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

private:
    void createCommandBuffers();
    void freeCommandBuffers();
    void recreateSwapChain();

    std::string getTimeStampStr(); // todo: take out to standalone logging system

    WrpWindow& wrpWindow;
    WrpDevice& wrpDevice;
    std::unique_ptr<WrpSwapChain> wrpSwapChain;
    std::vector<VkCommandBuffer> commandBuffers;

    uint32_t currentImageIndex;
    int currentFrameIndex{ 0 };           // [0, Max_Frames_In_Flight]
    bool isFrameStarted{ false };
};

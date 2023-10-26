#pragma once

#include "../Pipeline.hpp"
#include "../Device.hpp"
#include "../SceneObject.hpp"
#include "../Camera.hpp"
#include "../FrameInfo.hpp"

// std
#include <memory>
#include <vector>

class SimpleRenderSystem
{
public:
    SimpleRenderSystem(WrpDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~SimpleRenderSystem();

    SimpleRenderSystem(const SimpleRenderSystem&) = delete;
    SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;

    void renderSceneObjects(FrameInfo& frameInfo);

private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass);

    WrpDevice& wrpDevice;

    std::unique_ptr<WrpPipeline> wrpPipeline;
    VkPipelineLayout pipelineLayout;
};

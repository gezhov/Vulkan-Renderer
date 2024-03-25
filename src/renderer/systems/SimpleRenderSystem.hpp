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
    SimpleRenderSystem(WrpDevice& device, VkRenderPass renderPass,
        VkDescriptorSetLayout globalSetLayout, RenderingSettings& renderingSettings);
    ~SimpleRenderSystem();

    SimpleRenderSystem(const SimpleRenderSystem&) = delete;
    SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;

    void renderSceneObjects(FrameInfo& frameInfo);

private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    std::unique_ptr<WrpPipeline> createPipeline(VkRenderPass renderPass, int reflectionModel);

    WrpDevice& wrpDevice;
    RenderingSettings& renderingSettings;

    std::unique_ptr<WrpPipeline> wrpPipelineLambertian;
    std::unique_ptr<WrpPipeline> wrpPipelineBlinnPhong;
    VkPipelineLayout pipelineLayout;
};

#pragma once

#include "../Pipeline.hpp"
#include "../Device.hpp"
#include "../SceneObject.hpp"
#include "../Camera.hpp"
#include "../FrameInfo.hpp"
#include "../Renderer.hpp"

// std
#include <memory>
#include <vector>

class SimpleRenderSystem
{
public:
    SimpleRenderSystem(WrpDevice& device, WrpRenderer& renderer,
        VkDescriptorSetLayout globalSetLayout);
    ~SimpleRenderSystem();

    SimpleRenderSystem(const SimpleRenderSystem&) = delete;
    SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;

    void renderSceneObjects(FrameInfo& frameInfo);

private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    std::unique_ptr<WrpPipeline> createPipeline(
        VkRenderPass renderPass, int reflectionModel, int polygonFillMode);

    WrpDevice& wrpDevice;
    WrpRenderer& wrpRenderer;

    int curPlgnFillMode = 0;

    std::unique_ptr<WrpPipeline> wrpPipelineLambertian;
    std::unique_ptr<WrpPipeline> wrpPipelineBlinnPhong;
    std::unique_ptr<WrpPipeline> wrpPipelineTorranceSparrow;
    VkPipelineLayout pipelineLayout;
};

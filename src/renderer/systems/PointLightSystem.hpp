#pragma once

#include "../Pipeline.hpp"
#include "../Device.hpp"
#include "../GameObject.hpp"
#include "../Camera.hpp"
#include "../FrameInfo.hpp"

// std
#include <memory>
#include <vector>

ENGINE_BEGIN

class PointLightSystem
{
public:
    PointLightSystem(WrpDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~PointLightSystem();

    // Избавляемся от copy operator и copy constrcutor, т.к. PointLightSystem хранит в себе указатели
    // на VkPipelineLayout_T и VkCommandBuffer_T, которые лучше не копировать.
    PointLightSystem(const PointLightSystem&) = delete;
    PointLightSystem& operator=(const PointLightSystem&) = delete;

    void update(FrameInfo& frameInfo, GlobalUbo& ubo);
    void render(FrameInfo& frameInfo);

private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass);

    WrpDevice& wrpDevice;

    std::unique_ptr<WrpPipeline> wrpPipeline;
    VkPipelineLayout pipelineLayout;
};

ENGINE_END

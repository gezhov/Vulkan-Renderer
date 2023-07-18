#pragma once

#include "../vget_pipeline.hpp"
#include "../vget_device.hpp"
#include "../vget_game_object.hpp"
#include "../vget_camera.hpp"
#include "../vget_frame_info.hpp"

// std
#include <memory>
#include <vector>

namespace vget
{
    class SimpleRenderSystem
    {
    public:
        SimpleRenderSystem(VgetDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
        ~SimpleRenderSystem();

        SimpleRenderSystem(const SimpleRenderSystem&) = delete;
        SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;

        void renderGameObjects(FrameInfo& frameInfo);

    private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createPipeline(VkRenderPass renderPass);

        VgetDevice& vgetDevice;

        std::unique_ptr<VgetPipeline> vgetPipeline;
        VkPipelineLayout pipelineLayout;
    };
}

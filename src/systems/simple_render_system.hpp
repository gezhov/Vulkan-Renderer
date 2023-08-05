#pragma once

#include "../pipeline.hpp"
#include "../device.hpp"
#include "../game_object.hpp"
#include "../camera.hpp"
#include "../frame_info.hpp"

// std
#include <memory>
#include <vector>

ENGINE_BEGIN

class SimpleRenderSystem
{
public:
	SimpleRenderSystem(WrpDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
	~SimpleRenderSystem();

	SimpleRenderSystem(const SimpleRenderSystem&) = delete;
	SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;

	void renderGameObjects(FrameInfo& frameInfo);

private:
	void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
	void createPipeline(VkRenderPass renderPass);

	WrpDevice& vgetDevice;

	std::unique_ptr<WrpPipeline> vgetPipeline;
	VkPipelineLayout pipelineLayout;
};

ENGINE_END

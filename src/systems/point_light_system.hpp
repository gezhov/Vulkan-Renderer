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
	class PointLightSystem
	{
	public:
		PointLightSystem(VgetDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
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

		VgetDevice& vgetDevice;

		std::unique_ptr<VgetPipeline> vgetPipeline;
		VkPipelineLayout pipelineLayout;
	};
}
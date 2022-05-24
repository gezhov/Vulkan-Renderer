#pragma once

#include "../vget_pipeline.hpp"
#include "../vget_device.hpp"
#include "../vget_game_object.hpp"
#include "../vget_camera.hpp"
#include "../vget_frame_info.hpp"
#include "../vget_swap_chain.hpp"
#include "../vget_descriptors.hpp"

// std
#include <memory>
#include <vector>

namespace vget
{
	struct SimpleSystemUbo
	{
		alignas(16)int texturesCount;
	};

	class SimpleRenderSystem
	{
	public:
		SimpleRenderSystem(VgetDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout, FrameInfo frameInfo);
		~SimpleRenderSystem();

		// Избавляемся от copy operator и copy constrcutor, т.к. SimpleRenderSystem хранит в себе указатели
		// на VkPipelineLayout_T и VkCommandBuffer_T, которые лучше не копировать.
		SimpleRenderSystem(const SimpleRenderSystem&) = delete;
		SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;

		void renderGameObjects(FrameInfo& frameInfo);

	private:
		void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
		void createPipeline(VkRenderPass renderPass);
		void createUboBuffers();

		int fillModelsIds(VgetGameObject::Map& gameObjects);
		void createDescriptorSets(FrameInfo& frameInfo);

		VgetDevice& vgetDevice;

		std::unique_ptr<VgetPipeline> vgetPipeline;
		VkPipelineLayout pipelineLayout;

		std::vector<VgetGameObject::id_t> modelObjectsIds{};
		size_t prevModelCount = 0;
		std::vector<std::unique_ptr<VgetBuffer>> uboBuffers{ VgetSwapChain::MAX_FRAMES_IN_FLIGHT };

		std::unique_ptr<VgetDescriptorPool> simpleSystemPool;
		std::unique_ptr<VgetDescriptorSetLayout> simpleSystemSetLayout;
		std::vector<VkDescriptorSet> simpleSystemSets{ VgetSwapChain::MAX_FRAMES_IN_FLIGHT };
	};
}
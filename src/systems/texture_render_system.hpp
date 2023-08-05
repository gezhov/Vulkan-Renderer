#pragma once

#include "../pipeline.hpp"
#include "../device.hpp"
#include "../game_object.hpp"
#include "../camera.hpp"
#include "../frame_info.hpp"
#include "../swap_chain.hpp"
#include "../descriptors.hpp"

// std
#include <memory>
#include <vector>

ENGINE_BEGIN

class TextureRenderSystem
{
public:
	TextureRenderSystem(VgetDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout, FrameInfo frameInfo);
	~TextureRenderSystem();

	// Избавляемся от copy operator и copy constrcutor, т.к. TextureRenderSystem хранит в себе указатели
	// на VkPipelineLayout_T и VkCommandBuffer_T, которые лучше не копировать.
	TextureRenderSystem(const TextureRenderSystem&) = delete;
	TextureRenderSystem& operator=(const TextureRenderSystem&) = delete;

	void update(FrameInfo& frameInfo, TextureSystemUbo& ubo);
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

	std::unique_ptr<VgetDescriptorPool> systemDescriptorPool;
	std::unique_ptr<VgetDescriptorSetLayout> systemDescriptorSetLayout;
	std::vector<VkDescriptorSet> systemDescriptorSets{ VgetSwapChain::MAX_FRAMES_IN_FLIGHT };
};

ENGINE_END

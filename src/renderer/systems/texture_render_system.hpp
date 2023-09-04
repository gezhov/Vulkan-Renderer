#pragma once

#include "../pipeline.hpp"
#include "../device.hpp"
#include "../renderer.hpp"
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
    TextureRenderSystem(WrpDevice& device, WrpRenderer& renderer, VkDescriptorSetLayout globalSetLayout, FrameInfo frameInfo);
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

    int fillModelsIds(WrpGameObject::Map& sceneObjects);
    void createDescriptorSets(FrameInfo& frameInfo);
    void rewriteAndRecompileFragShader(int texturesCount);

    WrpDevice& wrpDevice;
    WrpRenderer& wrpRenderer;
    VkDescriptorSetLayout globalSetLayout;

    VkShaderModule fragShaderModule = nullptr;
    std::unique_ptr<WrpPipeline> wrpPipeline;
    VkPipelineLayout pipelineLayout;

    std::vector<WrpGameObject::id_t> modelObjectsIds{};
    size_t prevModelCount = 0;
    std::vector<std::unique_ptr<WrpBuffer>> uboBuffers{WrpSwapChain::MAX_FRAMES_IN_FLIGHT};

    std::unique_ptr<WrpDescriptorPool> systemDescriptorPool;
    std::unique_ptr<WrpDescriptorSetLayout> systemDescriptorSetLayout;
    std::vector<VkDescriptorSet> systemDescriptorSets{WrpSwapChain::MAX_FRAMES_IN_FLIGHT};
};

ENGINE_END

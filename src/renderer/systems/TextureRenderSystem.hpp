#pragma once

#include "../Pipeline.hpp"
#include "../Device.hpp"
#include "../Renderer.hpp"
#include "../SceneObject.hpp"
#include "../Camera.hpp"
#include "../FrameInfo.hpp"
#include "../SwapChain.hpp"
#include "../Descriptors.hpp"
#include "../ShaderModule.hpp"

// std
#include <memory>
#include <vector>

class TextureRenderSystem
{
public:
    TextureRenderSystem(WrpDevice& device, WrpRenderer& renderer,
        VkDescriptorSetLayout globalSetLayout, FrameInfo frameInfo);
    ~TextureRenderSystem();

    TextureRenderSystem(const TextureRenderSystem&) = delete;
    TextureRenderSystem& operator=(const TextureRenderSystem&) = delete;

    void renderSceneObjects(FrameInfo& frameInfo);

private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    std::unique_ptr<WrpPipeline> createPipeline(VkRenderPass renderPass, int reflectionModel, int polygonFillMode);

    int fillModelsIds(SceneObject::Map& sceneObjects);
    void createDescriptorSets(FrameInfo& frameInfo);
    void rewriteAndRecompileFragShader(ShaderModule*& shaderModule, std::string fragShaderName, int texturesCount);

    WrpDevice& wrpDevice;
    WrpRenderer& wrpRenderer;
    VkDescriptorSetLayout globalSetLayout;

    ShaderModule* fsModuleLambertian;
    ShaderModule* fsModuleBlinnPhong;
    ShaderModule* fsModuleTorranceSparrow;
    std::unique_ptr<WrpPipeline> wrpPipelineLambertian;
    std::unique_ptr<WrpPipeline> wrpPipelineBlinnPhong;
    std::unique_ptr<WrpPipeline> wrpPipelineTorranceSparrow;
    VkPipelineLayout pipelineLayout = nullptr;

    std::vector<SceneObject::id_t> modelObjectsIds{};
    size_t prevModelCount = 0;
    int curPlgnFillMode = 0;

    std::unique_ptr<WrpDescriptorPool> systemDescriptorPool;
    std::unique_ptr<WrpDescriptorSetLayout> systemDescriptorSetLayout;
    std::vector<VkDescriptorSet> systemDescriptorSets;
};

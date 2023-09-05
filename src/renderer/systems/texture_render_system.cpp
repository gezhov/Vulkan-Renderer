#include "texture_render_system.hpp"
#include "../buffer.hpp"

// libs
#define GLM_FORCE_RADIANS			  // Функции GLM будут работать с радианами, а не градусами
#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // GLM будет ожидать интервал нашего буфера глубины от 0 до 1 (например, для OpenGL используется интервал от -1 до 1)
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include "shaderc/shaderc.h"

// std
#include <stdexcept>
#include <cassert>
#include <array>
#include <iostream>
#include <fstream>

ENGINE_BEGIN

TextureRenderSystem::TextureRenderSystem(WrpDevice& device, WrpRenderer& renderer,
    VkDescriptorSetLayout globalSetLayout, FrameInfo frameInfo) : wrpDevice{device}, wrpRenderer{renderer}, globalSetLayout{globalSetLayout}
{
    createUboBuffers();
    prevModelCount = fillModelsIds(frameInfo.sceneObjects);
    createDescriptorSets(frameInfo);
    createPipelineLayout(globalSetLayout);
    createPipeline(renderer.getSwapChainRenderPass());
}

TextureRenderSystem::~TextureRenderSystem()
{
    vkDestroyPipelineLayout(wrpDevice.device(), pipelineLayout, nullptr);
}

void TextureRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
{
    // destroy the old one if it exists
    if (pipelineLayout != nullptr) vkDestroyPipelineLayout(wrpDevice.device(), pipelineLayout, nullptr);

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // access to push constanf from both VS and FS 
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(TextureSystemPushConstantData);

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout, systemDescriptorSetLayout->getDescriptorSetLayout()};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    if (vkCreatePipelineLayout(wrpDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout!");
    }
}

void TextureRenderSystem::createPipeline(VkRenderPass renderPass)
{
    assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

    PipelineConfigInfo pipelineConfig{};
    WrpPipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;

    wrpPipeline = std::make_unique<WrpPipeline>(
        wrpDevice,
        "src/renderer/shaders/texture_shader.vert.spv",
        "src/renderer/shaders/texture_shader.frag.spv",
        pipelineConfig,
        nullptr,
        fragShaderModule
    );
}

void TextureRenderSystem::createUboBuffers()
{
    for (int i = 0; i < uboBuffers.size(); ++i)
    {
        uboBuffers[i] = std::make_unique<WrpBuffer>(
            wrpDevice,
            sizeof(TextureSystemUbo),
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        uboBuffers[i]->map();
    }
}

int TextureRenderSystem::fillModelsIds(WrpGameObject::Map& sceneObjects)
{
    modelObjectsIds.clear();
    for (auto& kv : sceneObjects)
    {
        auto& obj = kv.second; // ссылка на объект из мапы
        if (obj.model != nullptr && obj.model->hasTextures == true) {
            modelObjectsIds.push_back(kv.first); // в этой системе рендерятся только объекты с текстурами
        }
    }
    return static_cast<int>(modelObjectsIds.size());
}

void TextureRenderSystem::createDescriptorSets(FrameInfo& frameInfo)
{
    int texturesCount = 0;
    std::vector<VkDescriptorImageInfo> descriptorImageInfos;

    for (auto& id : modelObjectsIds)
    {
        texturesCount += frameInfo.sceneObjects[id].model->getTextures().size();

        // Заполнение информации по дескрипторам текстур для каждой модели
        // todo сделать рефактор
        for (auto& texture : frameInfo.sceneObjects.at(id).model->getTextures())
        {
            if (texture != nullptr)
            {
                auto& imageInfo = texture->descriptorInfo();
                descriptorImageInfos.push_back(imageInfo);
            }
            else
            {
                // todo: добавление объектов с текстурами не будет корректно работать, пока не будет исправлен этот момент
                descriptorImageInfos.push_back(frameInfo.sceneObjects.at(id).model->getTextures().at(0)->descriptorInfo());
            }
        }
    }

    // wait for all of commands in graphics queue to complete before creating new descriptor pool and graphics pipeline eventually
    vkQueueWaitIdle(wrpDevice.graphicsQueue());

    WrpDescriptorPool::Builder poolBuilder = WrpDescriptorPool::Builder(wrpDevice)
        .setMaxSets(WrpSwapChain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, WrpSwapChain::MAX_FRAMES_IN_FLIGHT);
    if (texturesCount != 0) {
        poolBuilder.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, WrpSwapChain::MAX_FRAMES_IN_FLIGHT * texturesCount);
    }
    systemDescriptorPool = poolBuilder.build();

    WrpDescriptorSetLayout::Builder setLayoutBuilder = WrpDescriptorSetLayout::Builder(wrpDevice)
        .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    if (texturesCount != 0) {
        setLayoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, texturesCount);
    }
    systemDescriptorSetLayout = setLayoutBuilder.build();

    for (int i = 0; i < systemDescriptorSets.size(); ++i)
    {
        // Запись количества текстур для каждого ubo буфера
        TextureSystemUbo ubo{};
        uboBuffers[i]->writeToBuffer(&ubo);

        auto bufferInfo = uboBuffers[i]->descriptorInfo();

        WrpDescriptorWriter descriptorWriter = WrpDescriptorWriter(*systemDescriptorSetLayout, *systemDescriptorPool)
            .writeBuffer(0, &bufferInfo);
        if (texturesCount != 0) {
            descriptorWriter.writeImage(1, descriptorImageInfos.data(), texturesCount);
        }
        descriptorWriter.build(systemDescriptorSets[i]);
    }

    // перезапись и перекомпиляция шейдера фрагментов с новым значением количества текстур
    if (texturesCount != 0)
        rewriteAndRecompileFragShader(texturesCount);
}

void TextureRenderSystem::rewriteAndRecompileFragShader(int texturesCount)
{
    std::fstream shaderFile;
    std::string shaderContent;
    std::string line;
    std::string macros = "#define TEXTURES_COUNT ";
    static bool isTexturesDefined = false;

    shaderFile.open(ENGINE_DIR"src/renderer/shaders/texture_shader.frag", std::ios::in | std::ios::app);
    if (shaderFile.is_open())
    {
        while (std::getline(shaderFile, line))
        {
            if (line.find(macros) != std::string::npos)
            {
                // replacing TEXTURES_COUNT
                macros += std::to_string(texturesCount);
                line.replace(0, line.length(), macros);
                shaderContent += line + '\n';

                // single-time adding TEXTURES define and sampler2D array to activate code with texturing
                if (!isTexturesDefined) {
                    shaderContent += "#define TEXTURES\n";
                    shaderContent += "layout(set = 1, binding = 1) uniform sampler2D texSampler[TEXTURES_COUNT]; // Combined Image Sampler дескрипторы\n";
                }
                std::cout << line << std::endl;
            }
            else
            {
                shaderContent += line + '\n';
            }
        }
    }
    else std::cerr << "Your file couldn't be opened";
    shaderFile.close();

    // generated shader are rewriting every time with new textures count
    shaderFile.open(ENGINE_DIR"src/renderer/shaders/texture_shader_generated.frag", std::ios::out | std::ios::trunc);
    if (shaderFile.is_open())
    {
        shaderFile << shaderContent;
    }
    shaderFile.close();

    std::vector<char> fragShader = WrpPipeline::readFile("src/renderer/shaders/texture_shader_generated.frag");
    shaderc_compiler_t compiler = shaderc_compiler_initialize();
    shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler, fragShader.data(), fragShader.size(),
        shaderc_glsl_fragment_shader, ENGINE_DIR"src/renderer/shaders/texture_shader_generated.frag", "main", nullptr);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderc_result_get_length(result);
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderc_result_get_bytes(result));
    if (vkCreateShaderModule(wrpDevice.device(), &createInfo, nullptr, &fragShaderModule) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module.");

    shaderc_result_release(result);
    shaderc_compiler_release(compiler);
}

void TextureRenderSystem::update(FrameInfo& frameInfo, TextureSystemUbo& ubo)
{
    uboBuffers[frameInfo.frameIndex]->writeToBuffer(&ubo);
}

void TextureRenderSystem::renderGameObjects(FrameInfo& frameInfo)
{
    // Заполняется вектор идентификаторов объектов с текстурами, и если их кол-во изменилось, то
    // наборы дескрипторов для этих объектов пересоздаются, а вместе с ними и пайплайн, т.к. изменяется его схема.
    if (prevModelCount != fillModelsIds(frameInfo.sceneObjects)) {
        createDescriptorSets(frameInfo);
        createPipelineLayout(globalSetLayout);
        createPipeline(wrpRenderer.getSwapChainRenderPass());
    }
    prevModelCount = modelObjectsIds.size();

    wrpPipeline->bind(frameInfo.commandBuffer);  // прикрепление графического пайплайна к буферу команд

    std::vector<VkDescriptorSet> descriptorSets{ frameInfo.globalDescriptorSet, systemDescriptorSets[frameInfo.frameIndex] };
    // Привязываем наборы дескрипторов к пайплайну
    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
        0, 2, descriptorSets.data(), 0, nullptr
    );

    int textureIndexOffset = 0; // отступ в массиве текстур для текущего объекта
    for (auto& id : modelObjectsIds)
    {
        auto& obj = frameInfo.sceneObjects[id];

        TextureSystemPushConstantData push{};
        push.modelMatrix = obj.transform.mat4();
        push.normalMatrix = obj.transform.normalMatrix();

        // прикрепление буфера вершин (модели) и буфера индексов к буферу команд (создание привязки)
        obj.model->bind(frameInfo.commandBuffer);

        // Отрисовка каждого подобъекта .obj модели по отдельности с передачей своего индекса текстуры
        for (auto& info : obj.model->getSubObjectsInfos())
        {
            // Передача в пуш константу индекса текстуры. Если её нет у данного подобъекта, то будет передано -1.
            if (obj.model->getTextures().at(info.textureIndex) != nullptr)
                push.textureIndex = textureIndexOffset + info.textureIndex;
            else
            {
                push.textureIndex = -1;
                push.diffuseColor = info.diffuseColor;	
            }

            vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(TextureSystemPushConstantData), &push
            );

            // отрисовка буфера вершин
            obj.model->drawIndexed(frameInfo.commandBuffer, info.indexCount, info.indexStart);
        }
        textureIndexOffset += obj.model->getTextures().size();
    }
}

ENGINE_END

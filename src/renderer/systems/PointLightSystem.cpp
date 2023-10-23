#include "PointLightSystem.hpp"

// libs
#define GLM_FORCE_RADIANS			  // Функции GLM будут работать с радианами, а не градусами
#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // GLM будет ожидать интервал нашего буфера глубины от 0 до 1 (например, для OpenGL используется интервал от -1 до 1)
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <stdexcept>
#include <cassert>
#include <array>
#include <map>

ENGINE_BEGIN

struct PointLightPushConstants
{
    glm::vec4 position{};
    glm::vec4 color{};
    float radius{};
};

PointLightSystem::PointLightSystem(WrpDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
    : wrpDevice{device}
{
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);
}

PointLightSystem::~PointLightSystem()
{
    vkDestroyPipelineLayout(wrpDevice.device(), pipelineLayout, nullptr);
}

void PointLightSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
{
    // описание диапазона пуш-констант
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // доступ к данным констант из обоих шейдеров
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PointLightPushConstants);

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout}; // вектор используемых схем для наборов дескрипторов

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    if (vkCreatePipelineLayout(wrpDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }
}

void PointLightSystem::createPipeline(VkRenderPass renderPass)
{
    assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

    PipelineConfigInfo pipelineConfig{};
    WrpPipeline::defaultPipelineConfigInfo(pipelineConfig);
    WrpPipeline::enableAlphaBlending(pipelineConfig);
    pipelineConfig.bindingDescriptions.clear();   // массивы с привязками и атрибутами буфера вершин очищаем, т.к.
    pipelineConfig.attributeDescriptions.clear(); // они не нужны в PointLightSystem с билбордами
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;

    wrpPipeline = std::make_unique<WrpPipeline>(
        wrpDevice,
        "src/renderer/shaders/PointLight.vert.spv",
        "src/renderer/shaders/PointLight.frag.spv",
        pipelineConfig);
}

void PointLightSystem::update(FrameInfo& frameInfo, GlobalUbo& ubo)
{
    // матрица преобразования для вращения объектов точечного света
    auto rotateLight = glm::rotate(
        glm::mat4(1.f),	 // инициализируем единичную матрицу
        frameInfo.frameTime, // угол положения объекта изменяется пропорционально прошедшему между кадрами времени
        {0.f, -1.f, 0.f} // ось вращения (y == -1, значит вращение вокруг Up-вектора)
    );

    int lightIndex = 0;
    for (auto& kv : frameInfo.sceneObjects)
    {
        auto& obj = kv.second;
        if (obj.pointLight == nullptr) continue;

        assert(lightIndex < MAX_LIGHTS && "Point Lights exceed maximum specified");

        // обновление позиции PointLight'а в карусели, если она включена
        if (obj.pointLight->carouselEnabled == true)
            obj.transform.translation = glm::vec3(rotateLight * glm::vec4(obj.transform.translation, 1.f));

        // копируем текущие данные об объекте Point Light'а в Ubo структуру
        ubo.pointLights[lightIndex].position = glm::vec4(obj.transform.translation, 1.f);
        ubo.pointLights[lightIndex].color = glm::vec4(obj.color, obj.pointLight->lightIntensity);

        lightIndex += 1;
    }

    ubo.numLights = lightIndex;
}

void PointLightSystem::render(FrameInfo& frameInfo)
{
    // Автоматическа сортировка PointLight'ов в мапе по их дистанции до камеры.
    // Это нужно для поочерёдного порядка их отрисовки, начиная с дальних билбордов,
    // а затем для их дальнейшего правильного смешивания цветов в ColorBlend этапе.
    std::map<float, SceneObject::id_t> sorted;
    for (auto& kv : frameInfo.sceneObjects)
    {
        auto& obj = kv.second;
        if (obj.pointLight == nullptr) continue;

        // вычисление дистанции до камеры
        auto offset = frameInfo.camera.getPosition() - obj.transform.translation;
        float disSquared = glm::dot(offset, offset);
        sorted[disSquared] = obj.getId();
    }

    // render objects
    wrpPipeline->bind(frameInfo.commandBuffer);  // прикрепление графического пайплайна к буферу команд

    // привязываем набор дескрипторов к пайплайну
    vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0,
        1,
        &frameInfo.globalDescriptorSet,
        0,
        nullptr
    );

    // Отрисовываем билборды поинт лайтов в обратном порядке (от самого дальнего, до самого близкого к камере)
    for (auto it = sorted.rbegin(); it != sorted.rend(); ++it)
    {
        auto& obj = frameInfo.sceneObjects.at(it->second);

        PointLightPushConstants push{};
        push.position = glm::vec4(obj.transform.translation, 1.f);
        push.color = glm::vec4(obj.color, obj.pointLight->lightIntensity);
        push.radius = obj.transform.scale.x;

        vkCmdPushConstants(
            frameInfo.commandBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(PointLightPushConstants),
            &push
        );
        vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
    }
}

ENGINE_END

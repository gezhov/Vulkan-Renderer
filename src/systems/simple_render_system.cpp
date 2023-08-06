#include "simple_render_system.hpp"

// libs
#define GLM_FORCE_RADIANS			  // Функции GLM будут работать с радианами, а не градусами
#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // GLM будет ожидать интервал нашего буфера глубины от 0 до 1 (например, для OpenGL используется интервал от -1 до 1)
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <stdexcept>
#include <cassert>
#include <array>

ENGINE_BEGIN

// структура пуш-константы здесь объявлена временно
struct SimplePushConstantData
{
	glm::mat4 modelMatrix{ 1.f }; // такой конструктор создаёт единичную матрицу
	glm::mat4 normalMatrix{ 1.f };
	alignas(16) glm::vec3 diffuseColor{};
};

SimpleRenderSystem::SimpleRenderSystem(WrpDevice& device, VkRenderPass renderPass,
	VkDescriptorSetLayout globalDescriptorSetLayout) : wrpDevice{ device }
{
	createPipelineLayout(globalDescriptorSetLayout);
	createPipeline(renderPass);
}

SimpleRenderSystem::~SimpleRenderSystem()
{
	vkDestroyPipelineLayout(wrpDevice.device(), pipelineLayout, nullptr);
}

void SimpleRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalDescriptorSetLayout)
{
	// описание диапазона пуш-констант
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // доступ к данным констант из обоих шейдеров
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(SimplePushConstantData);

	// используемые схемы наборов дескрипторов
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalDescriptorSetLayout};

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	// DescriptorSetLayout объекты наборы дескрипторов, которые будут использоваться в шейдере.
	// Дескрипторы используются для передачи доп. данных в шейдеры (текстуры или Uniform Buffer объекты).
	// По одной и той же привязке в шейдер может быть передано сразу несколько сетов,
	// если в массиве pSetLayouts будет несколько соответствующих лэйаутов для этих сэтов.
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
	// PushConstant'ы используются для передачи в шейдерные программы небольшого количества данных.
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	if (vkCreatePipelineLayout(wrpDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create pipeline layout!");
	}
}

void SimpleRenderSystem::createPipeline(VkRenderPass renderPass)
{
	assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout.");

	PipelineConfigInfo pipelineConfig{};
	WrpPipeline::defaultPipelineConfigInfo(pipelineConfig);
	pipelineConfig.renderPass = renderPass;
	pipelineConfig.pipelineLayout = pipelineLayout;

	vgetPipeline = std::make_unique<WrpPipeline>(
		wrpDevice,
		"./shaders/simple_shader.vert.spv",
		"./shaders/simple_shader.frag.spv",
		pipelineConfig);
}

void SimpleRenderSystem::renderGameObjects(FrameInfo& frameInfo)
{
	vgetPipeline->bind(frameInfo.commandBuffer);  // прикрепление графического пайплайна к буферу команд

	// привязываем набор дескрипторов к пайплайну
	vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);

	for (auto& kv : frameInfo.gameObjects)
	{
		auto& obj = kv.second; // ссылка на объект из мапы

		// В данной системе рендерятся только объекты с моделями без материала (и, соответственно, текстур)
		if (obj.model == nullptr || obj.model->hasTextures == true) continue;

		SimplePushConstantData push{};
		push.modelMatrix = obj.transform.mat4();
		push.normalMatrix = obj.transform.normalMatrix();

		for (auto& info : obj.model->getSubObjectsInfo())
		{
			push.diffuseColor = info.diffuseColor;

			vkCmdPushConstants(
				frameInfo.commandBuffer,
				pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(SimplePushConstantData),
				&push);

			// прикрепление буфера вершин (модели) и буфера индексов к буферу команд (создание привязки)
			obj.model->bind(frameInfo.commandBuffer);
			// отрисовка буфера вершин
			obj.model->drawIndexed(frameInfo.commandBuffer, info.indexCount, info.indexStart);
		}

		// для простейших obj без подобъектов
		if (obj.model->getSubObjectsInfo().size() == 1) {
			push.diffuseColor = {1.f, 1.f, 1.f};

			vkCmdPushConstants(
				frameInfo.commandBuffer,
				pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(SimplePushConstantData),
				&push);

			obj.model->bind(frameInfo.commandBuffer);
			obj.model->draw(frameInfo.commandBuffer);
		}
	}
}

ENGINE_END

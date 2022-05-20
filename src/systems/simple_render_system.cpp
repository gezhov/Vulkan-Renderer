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

namespace vget
{
	// структура пуш-константы здесь объявлена временно
	struct SimplePushConstantData
	{
		glm::mat4 modelMatrix{ 1.f }; // такой конструктор создаёт единичную матрицу
		glm::mat4 normalMatrix{1.f};
		// далее идёт превышение минимально возможного размера данных для пуш константы => в потенциале нужно исправить этот момент
		int textureIndex;
		alignas(16) glm::vec3 diffuseColor{};
	};

	SimpleRenderSystem::SimpleRenderSystem(VgetDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
		: vgetDevice{device}
	{
		createPipelineLayout(globalSetLayout);
		createPipeline(renderPass);
	}

	SimpleRenderSystem::~SimpleRenderSystem()
	{
		vkDestroyPipelineLayout(vgetDevice.device(), pipelineLayout, nullptr);
	}

	void SimpleRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
	{
		// описание диапазона пуш-констант
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // доступ к данным констант из обоих шейдеров
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(SimplePushConstantData);

		std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout}; // вектор используемых схем для наборов дескрипторов

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		// DescriptorSetLayout объекты используются для передачи данных, отличных от данных о вершинах, в шейдеры.
		// Это могут быть текстуры или Uniform Buffer объекты.
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
		pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
		// PushConstant'ы используются для передачи в шейдерные программы небольшого количества данных.
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(vgetDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline layout!");
		}
	}

	void SimpleRenderSystem::createPipeline(VkRenderPass renderPass)
	{
		assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

		PipelineConfigInfo pipelineConfig{};
		VgetPipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;

		vgetPipeline = std::make_unique<VgetPipeline>(
			vgetDevice,
			"./shaders/simple_shader.vert.spv",
			"./shaders/simple_shader.frag.spv",
			pipelineConfig);
	}

	void SimpleRenderSystem::renderGameObjects(FrameInfo& frameInfo)
	{
		// render objects
		vgetPipeline->bind(frameInfo.commandBuffer);  // прикрепление графического пайплайна к буферу команд

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

		for (auto& kv : frameInfo.gameObjects)
		{
			auto& obj = kv.second; // ссылка на объект из мапы
			if (obj.model == nullptr) continue; // данная система пропускает объекты без модели

			SimplePushConstantData push{};
			push.modelMatrix = obj.transform.mat4();
			push.normalMatrix = obj.transform.normalMatrix();

			// Отрисовка каждого подобъекта .obj модели по отдельности с передачей своего индекса текстуры
			for (auto& info : obj.model->getSubObjectsInfo())
			{
				// Передача в пуш константу индекса текстуры. Если её нет у данного подобъекта, то будет передано -1.
				if (obj.model->getTextures().at(info.textureIndex) != nullptr)
					push.textureIndex = info.textureIndex;
				else
				{
					push.textureIndex = -1;
					push.diffuseColor = info.diffuseColor;	
				}

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
		}
	}
}
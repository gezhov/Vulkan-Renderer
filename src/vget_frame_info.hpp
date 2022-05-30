#pragma once

#include "vget_camera.hpp"
#include "vget_game_object.hpp"

// lib
#include <vulkan/vulkan.h>

namespace vget
{
#define MAX_LIGHTS 10

	struct PointLight
	{
		glm::vec4 position{}; // w - игнорируется
		glm::vec4 color{};	  // w - интенсивность цвета
	};

	// Структура, хранящая нужную для отрисовки кадра информацию.
	// Используется для удобной передачи множества аргументов в функции отрисовки.
	struct FrameInfo
	{
		int frameIndex;
		float frameTime;
		VkCommandBuffer commandBuffer;
		VgetCamera &camera;
		VkDescriptorSet globalDescriptorSet;
		VkDescriptorSetLayout globalSetLayout;
		VkRenderPass renderPass;
		VgetGameObject::Map& gameObjects;
	};

	struct GlobalUbo // global uniform buffer object
	{
		glm::mat4 projection{ 1.f };
		glm::mat4 view{ 1.f };
		glm::mat4 inverseView{ 1.f };
		//alignas(16) glm::vec3 lightDirection = glm::normalize(glm::vec3{1.f, -3.f, -1.f});
		glm::vec4 ambientLightColor{ 1.f, 1.f, 1.f, .02f }; // [r, g, b, w]
		PointLight pointLights[MAX_LIGHTS];
		int numLights; // кол-во активных точечных источников света
	};

	struct SimpleSystemUbo
	{
		//alignas(16)int texturesCount;
		int texturesCount;
		float directionalLightIntensity;
		alignas(16) glm::vec4 directionalLightPosition;
	};
}
#pragma once

#include "camera.hpp"
#include "game_object.hpp"

// lib
#include <vulkan/vulkan.h>

ENGINE_BEGIN

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
	WrpCamera& camera;
	VkDescriptorSet globalDescriptorSet;
	WrpGameObject::Map& sceneObjects;
};

struct GlobalUbo // global uniform buffer object
{
    // maxUniformBufferRange 65536 (for my device)

	glm::mat4 projection{ 1.f };
	glm::mat4 view{ 1.f };
	glm::mat4 inverseView{ 1.f };
	//alignas(16) glm::vec3 lightDirection = glm::normalize(glm::vec3{1.f, -3.f, -1.f});
	glm::vec4 ambientLightColor{ 1.f, 1.f, 1.f, .02f }; // [r, g, b, w]
	float directionalLightIntensity;
	alignas(16) glm::vec4 directionalLightPosition;
	PointLight pointLights[MAX_LIGHTS];
	int numLights; // кол-во активных точечных источников света
};

struct SimplePushConstantData
{
    glm::mat4 modelMatrix{ 1.f }; // такой конструктор создаёт единичную матрицу
    glm::mat4 normalMatrix{ 1.f };
    alignas(16) glm::vec3 diffuseColor{};
};

struct TextureSystemPushConstantData
{
    glm::mat4 modelMatrix{ 1.f };
    glm::mat4 normalMatrix{1.f};
    // 128 byte min limit is reached. next fields will be limited by maxPushConstantSize
    int textureIndex;
    alignas(16) glm::vec3 diffuseColor{};
};

ENGINE_END

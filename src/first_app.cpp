#include "first_app.hpp"

#include "systems/simple_render_system.hpp"
#include "systems/point_light_system.hpp"
#include "vget_camera.hpp"
#include "keyboard_movement_controller.hpp"
#include "vget_buffer.hpp"

// libs
#define GLM_FORCE_RADIANS			  // Функции GLM будут работать с радианами, а не градусами
#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // GLM будет ожидать интервал нашего буфера глубины от 0 до 1 (например, для OpenGL используется интервал от -1 до 1)
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <stdexcept>
#include <cassert>
#include <array>
#include <chrono>
#include <numeric>

#define MAX_FRAME_TIME 0.5f

namespace vget
{
	FirstApp::FirstApp()
	{
		// Создаётся глобальный пул дескрипторов для всего приложения
		globalPool = VgetDescriptorPool::Builder(vgetDevice)
			.setMaxSets(VgetSwapChain::MAX_FRAMES_IN_FLIGHT)
			.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VgetSwapChain::MAX_FRAMES_IN_FLIGHT)
			.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VgetSwapChain::MAX_FRAMES_IN_FLIGHT)
			.build();

		loadGameObjects();
	}

	FirstApp::~FirstApp() {}

	void FirstApp::run()
	{
		// Создание Uniform Buffer'ов. По одному на каждый одновременно рисующийся кадр.
		std::vector<std::unique_ptr<VgetBuffer>> uboBuffers(VgetSwapChain::MAX_FRAMES_IN_FLIGHT);
		for (int i = 0; i < uboBuffers.size(); ++i)
		{
			uboBuffers[i] = std::make_unique<VgetBuffer> (
				vgetDevice,
				sizeof(GlobalUbo),
				1,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				// Здесь не исп. HOST_COHERENT свойство, чтобы продемонстрировать flush сброс памяти в девайс
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
			);
			uboBuffers[i]->map();
		}

		// Создаётся глобальная схема набора дескрипторов (действует на всё приложение)
		auto globalSetLayout = VgetDescriptorSetLayout::Builder(vgetDevice)
			.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
			.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build();

		// Выделение наборов дескрипторов из пула с информацией по Uniform Buffer'ам
		std::vector<VkDescriptorSet> globalDescriptorSets(VgetSwapChain::MAX_FRAMES_IN_FLIGHT);
		for (int i = 0; i < globalDescriptorSets.size(); ++i)
		{
			auto& imageInfo = gameObjects.at(0).model->descriptorInfo(); // todo рефактор

			auto bufferInfo = uboBuffers[i]->descriptorInfo();
			VgetDescriptorWriter(*globalSetLayout, *globalPool)
				.writeBuffer(0, &bufferInfo)
				.writeImage(1, &imageInfo)
				.build(globalDescriptorSets[i]);
		}

		SimpleRenderSystem simpleRenderSystem{
			vgetDevice,
			vgetRenderer.getSwapChainRenderPass(),
			globalSetLayout->getDescriptorSetLayout()
		};
		PointLightSystem pointLightSystem{
			vgetDevice,
			vgetRenderer.getSwapChainRenderPass(),
			globalSetLayout->getDescriptorSetLayout()
		};
		VgetCamera camera{};
		// установка положения "теоретической камеры"
		//camera.setViewDirection(glm::vec3{0.f}, glm::vec3{0.5f, 0.f, 1.f});
		//camera.setViewTarget(glm::vec3{-3.f, -3.f, 23.f}, {.0f, .0f, 1.5f});

		auto viewerObject = VgetGameObject::createGameObject(); // объект без модели для хранения текущего состояния камеры
		viewerObject.transform.translation.z = -2.5f; // начальное положение сдвинуто немного назад
		KeyboardMovementController cameraController{};

		auto currentTime = std::chrono::high_resolution_clock::now();

		// Обработка событий происходит, пока окно не должно быть закрыто.
		while (!vgetWindow.shouldClose())
		{
			glfwPollEvents(); // Обработка событий из очереди (нажатие клавиш, взаимодействие с окном и т.п.)

			// расчёт временного шага с момента последней итерации
			auto newTime = std::chrono::high_resolution_clock::now();
			float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
			currentTime = newTime;

			// Ограничение на макс. время кадра. Было добавлено для случая, когда камера сдвигалась слишком далеко,
			// потому что была зажата клавиша, а экран находился в режиме ресайза (время кадра становилось большим).
			frameTime = glm::min(frameTime, MAX_FRAME_TIME);

			// двигаем/вращаем объект теоретической камеры в зависимости от ввода с клавиатуры
			cameraController.moveInPlaneXZ(vgetWindow.getGLFWwindow(), frameTime, viewerObject);
			camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

			// Матрица ортогонального проецирования перестраивается каждый кадр, чтобы размеры ортогонального объёма просмотра
			// всегда соответствовали текущему значению соотношения сторон окна.
			// Aspect ratio подставляется именно в -left и right, чтобы соответствовать выражению: right - left = aspect * (bottom - top)
			// И в таком случае ортогональный объём просмотра будет иметь такое же соотношение сторон, что и окно.
			// Это избавляет отображаемый объект от искажений, связанных с соотношением сторон.
			float aspect = vgetRenderer.getAspectRatio();
			//camera.setOrthographicProjection(-aspect, aspect, -1, 1, -1, 1);

			// Установка матрицы проецирования перспективы.
			// Первый аргумент - vertical field of view (обычно в диапазоне от 45 до 60 градусов), далее соотношение сторон окна,
			// далее расстояние до ближней и затем дальней плоскости отсечения.
			// Главное отличие от простого ортогонального проецирования - объект становится тем меньше, чем он дальше от передней плоскости.
			// Это тип проецирования чаще всего используется в играх.
			camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

			// отрисовка кадра
			if (auto commandBuffer = vgetRenderer.beginFrame()) // beginFrame() вернёт nullptr, если требуется пересоздание SwapChain'а
			{
				int frameIndex = vgetRenderer.getFrameIndex();
				FrameInfo frameInfo {frameIndex, frameTime, commandBuffer, camera, globalDescriptorSets[frameIndex], gameObjects};

				// UPDATE SECTION
				// Обновление данных внутри uniform buffer объекта для текущего кадра
				GlobalUbo ubo{};
				ubo.projection = camera.getProjection();
				ubo.view = camera.getView();
				ubo.inverseView = camera.getInverseView();
				pointLightSystem.update(frameInfo, ubo);
				uboBuffers[frameIndex]->writeToBuffer(&ubo);
				uboBuffers[frameIndex]->flush();

				// RENDER SECTION
				/* Начало и конец прохода рендера и кадра отделены друг от друга для упрощения в дальнейшем
				   интеграции сразу нескольких проходов рендера (Render passes) для создания отражений,
				   теней и эффектов пост-процесса. */
				vgetRenderer.beginSwapChainRenderPass(commandBuffer);
				// Порядок отрисовки объектов важен, так как сначала надо отрисовать непрозрачные объекты с помощью SimpleRenderSystem, а
				// затем полупрозрачные билборды поинт лайтов с помощью PointLightSystem.
				simpleRenderSystem.renderGameObjects(frameInfo);
				pointLightSystem.render(frameInfo);
				vgetRenderer.endSwapChainRenderPass(commandBuffer);
				vgetRenderer.endFrame();
			}
		}

		vkDeviceWaitIdle(vgetDevice.device());  // ожидать завершения всех операций на GPU перед закрытием программы и очисткой всех ресурсов
	}

	void FirstApp::loadGameObjects()
	{
		// Viking Room model
		std::shared_ptr<VgetModel> vikingRoom = VgetModel::createModelFromFile(vgetDevice, "models/viking_room.obj");
		vikingRoom->createTextureImage();
		vikingRoom->createTextureImageView();
		vikingRoom->createTextureSampler();
		auto vikingRoomObj = VgetGameObject::createGameObject();
		vikingRoomObj.model = vikingRoom;
		vikingRoomObj.transform.translation = {.0f, .0f, 0.f};
		vikingRoomObj.transform.scale = glm::vec3(1.f, 1.f, 1.f);
		vikingRoomObj.transform.rotation = glm::vec3(1.57f, 2.f, 0.f);
		gameObjects.emplace(vikingRoomObj.getId(), std::move(vikingRoomObj));

		// тестирование работы текстурирования
		/*VgetModel::Builder textureModelBuilder{};
		const std::vector<VgetModel::Vertex> vertices = {
			{{-0.5f, -0.5f, 0.f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
			{{0.5f, -0.5f, 0.f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
			{{0.5f, 0.5f, 0.f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
			{{-0.5f, 0.5f, 0.f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},

			{{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
			{{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
			{{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
			{{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
		};
		const std::vector<uint32_t> indices = {
			0, 1, 2, 2, 3, 0,
			4, 5, 6, 6, 7, 4
		};
		textureModelBuilder.vertices = vertices;
		textureModelBuilder.indices = indices;
		std::shared_ptr<VgetModel> vgetTextureModel = std::make_unique<VgetModel>(vgetDevice, textureModelBuilder);
		vgetTextureModel->createTextureImage();
		vgetTextureModel->createTextureImageView();
		vgetTextureModel->createTextureSampler();
		auto textureObject = VgetGameObject::createGameObject();
		textureObject.model = vgetTextureModel;
		textureObject.transform.translation = {.0f, -1.0f, 0.f};
		textureObject.transform.scale = glm::vec3(1.f, 1.f, 1.f);
		gameObjects.emplace(textureObject.getId(), std::move(textureObject));*/

		// сцена из vget
		//std::shared_ptr<VgetModel> vgetModel = VgetModel::createModelFromFile(vgetDevice, "models/flat_vase.obj");
		//auto flatVase = VgetGameObject::createGameObject();
		//flatVase.model = vgetModel;
		//flatVase.transform.translation = {-.5f, .5f, 0.f}; // в глубину объект двигается внутри ортогонального объёма просмотра, поэтому он не ограничен каноническим диапазоном [0;1]
		//flatVase.transform.scale = glm::vec3(3.f, 1.5f, 3.f);
		//gameObjects.emplace(flatVase.getId(), std::move(flatVase));

		//vgetModel = VgetModel::createModelFromFile(vgetDevice, "models/smooth_vase.obj");
		//auto smoothVase = VgetGameObject::createGameObject();
		//smoothVase.model = vgetModel;
		//smoothVase.transform.translation = {.5f, .5f, 0.f};
		//smoothVase.transform.scale = glm::vec3(3.f, 1.5f, 3.f);
		//gameObjects.emplace(smoothVase.getId(), std::move(smoothVase));

		//vgetModel = VgetModel::createModelFromFile(vgetDevice, "models/quad.obj");
		//auto floor = VgetGameObject::createGameObject();
		//floor.model = vgetModel;
		//floor.transform.translation = {0.f, .5f, 0.f};
		//floor.transform.scale = glm::vec3(3.f, 1.f, 3.f);
		//gameObjects.emplace(floor.getId(), std::move(floor));

		std::vector<glm::vec3> lightColors {
			{1.f, .1f, .1f},
			{.1f, .1f, 1.f},
			{.1f, 1.f, .1f},
			{1.f, 1.f, .1f},
			{.1f, 1.f, 1.f},
			{1.f, 1.f, 1.f}
		};

		// Создаётся по одному Point Light'у на каждый цвет
		for (int i = 0; i < lightColors.size(); ++i)
		{
			auto pointLight = VgetGameObject::makePointLight(0.2f);
			pointLight.color = lightColors[i];

			// создаётся матрица преобразования для расстановки объектов точечного света по кругу
			auto rotateLight = glm::rotate(
				glm::mat4(1.f),	// инициализируем единичную матрицу
				// каждый PointLight располагается под своим углом, который задан определённой частью от окружности (360 град. == 2*pi)
				(i * glm::two_pi<float>()) / lightColors.size(),
				{0.f, -1.f, 0.f} // ось вращения (y == -1, значит объекты будут расставлены вокруг Up-вектора)
			);

			pointLight.transform.translation = glm::vec3(rotateLight * glm::vec4(-1.f, -1.f, -1.f, 1.f));
			// само вращение реализовано в функции update() системы PointLightSystem

			gameObjects.emplace(pointLight.getId(), std::move(pointLight));
		}
	}
}
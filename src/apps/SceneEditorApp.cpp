#include "SceneEditorApp.hpp"

#include "SceneEditorGUI.hpp"
#include "../renderer/systems/SimpleRenderSystem.hpp"
#include "../renderer/systems/TextureRenderSystem.hpp"
#include "../renderer/systems/PointLightSystem.hpp"
#include "../renderer/Buffer.hpp"
#include "../renderer/Camera.hpp"
#include "./common/KeyboardMovementController.hpp"

// libs
#define GLM_FORCE_RADIANS			  // Функции GLM будут работать с радианами, а не градусами
#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // GLM будет ожидать интервал нашего буфера глубины от 0 до 1 (для OpenGL используется интервал от -1 до 1)
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

// std
#include <stdexcept>
#include <cassert>
#include <array>
#include <chrono>
#include <numeric>

#define MAX_FRAME_TIME 0.5f

SceneEditorApp::SceneEditorApp()
{
    // global descriptor pool designed for the entire app 
    globalPool = WrpDescriptorPool::Builder(wrpDevice)
        .setMaxSets(WrpSwapChain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, WrpSwapChain::MAX_FRAMES_IN_FLIGHT)
        .build();

    loadSceneObjects(); // load predefined objects
}

SceneEditorApp::~SceneEditorApp() {}

void SceneEditorApp::run()
{
    // Создание Uniform Buffer'ов. По одному на каждый одновременно рисующийся кадр.
    std::vector<std::unique_ptr<WrpBuffer>> uboBuffers(WrpSwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < uboBuffers.size(); ++i)
    {
        uboBuffers[i] = std::make_unique<WrpBuffer> (
            wrpDevice,
            sizeof(GlobalUbo),
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            // Здесь не исп. HOST_COHERENT свойство, чтобы продемонстрировать flush сброс памяти в девайс
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        );
        uboBuffers[i]->map();
    }

    // Создаётся глобальная схема набора дескрипторов (действует на всё приложение)
    auto globalDescriptorSetLayout = WrpDescriptorSetLayout::Builder(wrpDevice)
        .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS, 1)
        .build();

    // Выделение наборов дескрипторов из пула (по одному сету на кадр)
    std::vector<VkDescriptorSet> globalDescriptorSets(WrpSwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < globalDescriptorSets.size(); ++i)
    {
        VkDescriptorBufferInfo bufferInfo = uboBuffers[i]->descriptorInfo(); // инфа о буфере для дескриптора

        WrpDescriptorWriter(*globalDescriptorSetLayout, *globalPool)
            .writeBuffer(0, &bufferInfo)
            .build(globalDescriptorSets[i]);
    }

    SimpleRenderSystem simpleRenderSystem{
        wrpDevice,
        wrpRenderer.getSwapChainRenderPass(),
        globalDescriptorSetLayout->getDescriptorSetLayout()
    };
    TextureRenderSystem textureRenderSystem{
        wrpDevice,
        wrpRenderer,
        globalDescriptorSetLayout->getDescriptorSetLayout(),
        FrameInfo{0, 0, nullptr, WrpCamera{}, nullptr, sceneObjects}
    };
    PointLightSystem pointLightSystem{
        wrpDevice,
        wrpRenderer.getSwapChainRenderPass(),
        globalDescriptorSetLayout->getDescriptorSetLayout()
    };

    WrpCamera camera{};
    // default camera transform
    //camera.setViewDirection(glm::vec3{0.f}, glm::vec3{0.5f, 0.f, 1.f});
    //camera.setViewTarget(glm::vec3{-3.f, -3.f, 23.f}, {.0f, .0f, 1.5f});

    auto cameraObject = SceneObject::createSceneObject("Camera"); // объект без модели для хранения текущего состояния камеры
    cameraObject.transform.rotation = {.0f, .0f, .0f};
    sceneObjects.emplace(cameraObject.getId(), std::move(cameraObject));
    KeyboardMovementController cameraController{};

    SceneEditorGUI appGUI{
        wrpWindow,
        wrpDevice,
        wrpRenderer.getSwapChainRenderPass(),
        WrpSwapChain::MAX_FRAMES_IN_FLIGHT,
        camera,
        cameraController,
        sceneObjects
    };

    auto currentTime = std::chrono::high_resolution_clock::now();

    // MAIN LOOP
    while (!wrpWindow.shouldClose())
    {
        glfwPollEvents(); // Обработка событий из очереди (нажатие клавиш, взаимодействие с окном и т.п.)

        // расчёт временного шага с момента последней итерации
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
        currentTime = newTime;

        // Ограничение на макс. время кадра. Было добавлено для случая, когда камера сдвигалась слишком далеко,
        // потому что была зажата клавиша, а экран находился в режиме ресайза (время кадра становилось большим).
        frameTime = glm::min(frameTime, MAX_FRAME_TIME);

        // двигаем/вращаем объект камеры в зависимости от ввода с клавиатуры
        cameraController.moveInPlaneXZ(wrpWindow.getGLFWwindow(), frameTime, cameraObject);
        camera.setViewYXZ(cameraObject.transform.translation, cameraObject.transform.rotation);

        float aspect = wrpRenderer.getAspectRatio();
        //camera.setOrthographicProjection(-aspect, aspect, -1, 1, -1, 1);

        camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

        // отрисовка кадра
        if (auto commandBuffer = wrpRenderer.beginFrame()) // beginFrame() вернёт nullptr, если требуется пересоздание SwapChain'а
        {
            appGUI.newFrame(); // tell imgui that we're starting a new frame

            int frameIndex = wrpRenderer.getFrameIndex();
            FrameInfo frameInfo {frameIndex, frameTime, commandBuffer, camera,
                globalDescriptorSets[frameIndex], sceneObjects};

            // UPDATE SECTION
            GlobalUbo ubo{};
            ubo.projection = camera.getProjection();
            ubo.view = camera.getView();
            ubo.inverseView = camera.getInverseView();
            ubo.directionalLightIntensity = appGUI.directionalLightIntensity;
            ubo.directionalLightPosition = appGUI.directionalLightPosition;
            pointLightSystem.update(frameInfo, ubo);
            uboBuffers[frameIndex]->writeToBuffer(&ubo);
            uboBuffers[frameIndex]->flush();

            // RENDER SECTION
            wrpRenderer.beginSwapChainRenderPass(commandBuffer, appGUI.clearColor);

            // Order of objects render is matter, because we need to ensure that we rendered
            // fully opaque objects (like textured models) before rendering translucent objects (like point lights)
            simpleRenderSystem.renderSceneObjects(frameInfo);
            textureRenderSystem.renderSceneObjects(frameInfo);
            pointLightSystem.render(frameInfo);
            appGUI.setupGUI();
            appGUI.render(commandBuffer);

            wrpRenderer.endSwapChainRenderPass(commandBuffer);
            wrpRenderer.endFrame();
        }
    }

    vkDeviceWaitIdle(wrpDevice.device());
}

void SceneEditorApp::loadSceneObjects()
{
    // Viking Room model
   /* std::shared_ptr<WrpModel> vikingRoom = WrpModel::createModelFromObjTexture(
        wrpDevice, ENGINE_DIR"models/viking_room.obj", MODELS_DIR"textures/viking_room.png");
    auto vikingRoomObj = SceneObject::createSceneObject();
    vikingRoomObj.model = vikingRoom;
    vikingRoomObj.transform.translation = {.0f, .0f, 0.f};
    vikingRoomObj.transform.scale = glm::vec3(1.f, 1.f, 1.f);
    vikingRoomObj.transform.rotation = glm::vec3(1.57f, 2.f, 0.f);
    sceneObjects.emplace(vikingRoomObj.getId(), std::move(vikingRoomObj));*/

    // Sponza model
    std::shared_ptr<WrpModel> sponza = WrpModel::createModelFromObjMtl(wrpDevice, "../../../models/sponza.obj");
    auto sponzaObj = SceneObject::createSceneObject("Sponza");
    sponzaObj.model = sponza;
    sponzaObj.transform.translation = {-3.f, 1.0f, -2.f};
    sponzaObj.transform.scale = glm::vec3(0.01f, 0.01f, 0.01f);
    sponzaObj.transform.rotation = glm::vec3(3.15f, 0.f, 0.f);
    sceneObjects.emplace(sponzaObj.getId(), std::move(sponzaObj));
}

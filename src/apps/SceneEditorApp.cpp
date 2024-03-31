#include "SceneEditorApp.hpp"

#include "SceneEditorGUI.hpp"
#include "../renderer/systems/SimpleRenderSystem.hpp"
#include "../renderer/systems/TextureRenderSystem.hpp"
#include "../renderer/systems/PointLightSystem.hpp"
#include "../renderer/Buffer.hpp"
#include "../renderer/Camera.hpp"
#include "./common/KeyboardMovementController.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
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

SceneEditorApp::SceneEditorApp(int preloadScene)
{
    // global descriptor pool designed for the entire app 
    globalPool = WrpDescriptorPool::Builder(wrpDevice)
        .setMaxSets(WrpSwapChain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, WrpSwapChain::MAX_FRAMES_IN_FLIGHT)
        .build();

    if (preloadScene == 1) {
        loadScene1();
    } else if (preloadScene == 2) {
        loadScene2();
    }
}

SceneEditorApp::~SceneEditorApp() {}

void SceneEditorApp::run()
{
    // Uniform Buffers creation
    std::vector<std::unique_ptr<WrpBuffer>> uboBuffers(WrpSwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < uboBuffers.size(); ++i)
    {
        uboBuffers[i] = std::make_unique<WrpBuffer>(
            wrpDevice,
            sizeof(GlobalUbo),
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            // PROPERTY_HOST_COHERENT is not used to demonstrate explicit memory flush to device
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        );
        uboBuffers[i]->map();
    }

    // Global Descriptor Set Layout for the entire app
    auto globalDescriptorSetLayout = WrpDescriptorSetLayout::Builder(wrpDevice)
        .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS, 1)
        .build();

    // Getting Descriptor Sets from pool
    std::vector<VkDescriptorSet> globalDescriptorSets(WrpSwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < globalDescriptorSets.size(); ++i)
    {
        VkDescriptorBufferInfo bufferInfo = uboBuffers[i]->descriptorInfo(); // инфа о буфере для дескриптора

        WrpDescriptorWriter(*globalDescriptorSetLayout, *globalPool)
            .writeBuffer(0, &bufferInfo)
            .build(globalDescriptorSets[i]);
    }

    RenderingSettings renderingSettings{1, 0};

    SimpleRenderSystem simpleRenderSystem{
        wrpDevice,
        wrpRenderer,
        globalDescriptorSetLayout->getDescriptorSetLayout(),
    };
    TextureRenderSystem textureRenderSystem{
        wrpDevice,
        wrpRenderer,
        globalDescriptorSetLayout->getDescriptorSetLayout(),
        FrameInfo{0, 0, nullptr, WrpCamera{}, nullptr, sceneObjects, renderingSettings}
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

    // SceneObject for the editor camera 
    auto cameraObject = SceneObject::createSceneObject("Camera");
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
        sceneObjects,
        renderingSettings
    };

    auto currentTime = std::chrono::high_resolution_clock::now();

    // MAIN LOOP
    while (!wrpWindow.shouldClose())
    {
        glfwPollEvents(); // Process glfw events from queue

        // calculating frameTime and currentTime 
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
        currentTime = newTime;

        // Max frame time bound. For example, frameTime can become too long when window are in resizing mode.
        frameTime = glm::min(frameTime, MAX_FRAME_TIME);

        // Move/rotate camera corresponding to the input
        cameraController.moveInPlaneXZ(wrpWindow.getGLFWwindow(), frameTime, cameraObject);
        camera.setViewYXZ(cameraObject.transform.translation, cameraObject.transform.rotation);

        float aspect = wrpRenderer.getAspectRatio();
        //camera.setOrthographicProjection(-aspect, aspect, -1, 1, -1, 1);
        camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

        // frame rendering
        if (auto commandBuffer = wrpRenderer.beginFrame()) // beginFrame() will return nullptr if SwapChain recreation is needed
        {
            appGUI.newFrame(); // tell imgui that we're starting a new frame

            int frameIndex = wrpRenderer.getFrameIndex();
            FrameInfo frameInfo{frameIndex, frameTime, commandBuffer, camera,
                globalDescriptorSets[frameIndex], sceneObjects, renderingSettings};

            // UPDATE SECTION
            GlobalUbo ubo{};
            ubo.projection = camera.getProjection();
            ubo.view = camera.getView();
            ubo.inverseView = camera.getInverseView();
            ubo.directionalLightIntensity = appGUI.directionalLightIntensity;
            ubo.directionalLightPosition = appGUI.directionalLightPosition;
            ubo.diffuseProportion = appGUI.diffuseProportion;
            ubo.roughness = appGUI.roughness;
            ubo.indexOfRefraction = appGUI.indexOfRefraction;
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

void SceneEditorApp::loadScene1()
{
    // Viking Room model
    std::shared_ptr<WrpModel> vikingRoom = WrpModel::createModelFromObjTexture(
        wrpDevice, ENGINE_DIR"models/viking_room.obj", MODELS_DIR"textures/viking_room.png");
    auto vikingRoomObj = SceneObject::createSceneObject("VikingRoom");
    vikingRoomObj.model = vikingRoom;
    vikingRoomObj.transform.translation = {.0f, .0f, 0.f};
    vikingRoomObj.transform.scale = glm::vec3(1.f, 1.f, 1.f);
    vikingRoomObj.transform.rotation = glm::vec3(1.57f, 2.f, 0.f);
    sceneObjects.emplace(vikingRoomObj.getId(), std::move(vikingRoomObj));

    // Sponza model
    std::shared_ptr<WrpModel> sponza = WrpModel::createModelFromObjMtl(wrpDevice, "../../../models/sponza.obj");
    auto sponzaObj = SceneObject::createSceneObject("Sponza");
    sponzaObj.model = sponza;
    sponzaObj.transform.translation = {-3.f, 1.0f, -2.f};
    sponzaObj.transform.scale = glm::vec3(0.01f, 0.01f, 0.01f);
    sponzaObj.transform.rotation = glm::vec3(3.15f, 0.f, 0.f);
    sceneObjects.emplace(sponzaObj.getId(), std::move(sponzaObj));
}

void SceneEditorApp::loadScene2()
{
    std::shared_ptr<WrpModel> bunny = WrpModel::createModelFromObjMtl(wrpDevice, "../../../models/bunny.obj");
    auto bunnyObj = SceneObject::createSceneObject();
    bunnyObj.model = bunny;
    bunnyObj.transform.translation = {0.f, 0.f, 0.f};
    bunnyObj.transform.scale = glm::vec3(0.4f, 0.4f, 0.4f);
    bunnyObj.transform.rotation = glm::vec3(3.15f, 0.f, 0.f);
    sceneObjects.emplace(bunnyObj.getId(), std::move(bunnyObj));

    bunnyObj = SceneObject::createSceneObject();
    bunnyObj.model = bunny;
    bunnyObj.transform.translation = {1.f, 0.f, 1.f};
    bunnyObj.transform.scale = glm::vec3(0.4f, 0.4f, 0.4f);
    bunnyObj.transform.rotation = glm::vec3(3.15f, 0.f, 0.f);
    sceneObjects.emplace(bunnyObj.getId(), std::move(bunnyObj));

}


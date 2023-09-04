#include "application.hpp"

#include "../renderer/imgui.hpp"
#include "../renderer/systems/simple_render_system.hpp"
#include "../renderer/systems/texture_render_system.hpp"
#include "../renderer/systems/point_light_system.hpp"
#include "../renderer/camera.hpp"
#include "../renderer/keyboard_movement_controller.hpp"
#include "../renderer/buffer.hpp"

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

ENGINE_BEGIN

App::App()
{
    // Создаётся глобальный пул дескрипторов для всего приложения
    globalPool = WrpDescriptorPool::Builder(wrpDevice)
        .setMaxSets(WrpSwapChain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, WrpSwapChain::MAX_FRAMES_IN_FLIGHT)
        .build();

    loadSceneObjects(); // load predefined objects
}

App::~App() {}

void App::run()
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
    // установка положения "теоретической камеры"
    //camera.setViewDirection(glm::vec3{0.f}, glm::vec3{0.5f, 0.f, 1.f});
    //camera.setViewTarget(glm::vec3{-3.f, -3.f, 23.f}, {.0f, .0f, 1.5f});

    auto cameraObject = WrpGameObject::createGameObject("Camera"); // объект без модели для хранения текущего состояния камеры
    sceneObjects.emplace(cameraObject.getId(), std::move(cameraObject));
    KeyboardMovementController cameraController{};

    WrpImgui wrpImgui{
        wrpWindow,
        wrpDevice,
        wrpRenderer.getSwapChainRenderPass(),
        WrpSwapChain::MAX_FRAMES_IN_FLIGHT,
        camera,
        cameraController,
        sceneObjects
    };

    auto currentTime = std::chrono::high_resolution_clock::now();

    // Обработка событий происходит, пока окно не должно быть закрыто.
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

        // двигаем/вращаем объект теоретической камеры в зависимости от ввода с клавиатуры
        cameraController.moveInPlaneXZ(wrpWindow.getGLFWwindow(), frameTime, cameraObject);
        camera.setViewYXZ(cameraObject.transform.translation, cameraObject.transform.rotation);

        // Матрица ортогонального проецирования перестраивается каждый кадр, чтобы размеры ортогонального объёма просмотра
        // всегда соответствовали текущему значению соотношения сторон окна.
        // Aspect ratio подставляется именно в -left и right, чтобы соответствовать выражению: right - left = aspect * (bottom - top)
        // И в таком случае ортогональный объём просмотра будет иметь такое же соотношение сторон, что и окно.
        // Это избавляет отображаемый объект от искажений, связанных с соотношением сторон.
        float aspect = wrpRenderer.getAspectRatio();
        //camera.setOrthographicProjection(-aspect, aspect, -1, 1, -1, 1);

        // Установка матрицы проецирования перспективы.
        // Первый аргумент - vertical field of view (обычно в диапазоне от 45 до 60 градусов), далее соотношение сторон окна,
        // далее расстояние до ближней и затем дальней плоскости отсечения.
        // Главное отличие от простого ортогонального проецирования - объект становится тем меньше, чем он дальше от передней плоскости.
        // Это тип проецирования чаще всего используется в играх.
        camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

        // отрисовка кадра
        if (auto commandBuffer = wrpRenderer.beginFrame()) // beginFrame() вернёт nullptr, если требуется пересоздание SwapChain'а
        {
            wrpImgui.newFrame(); // tell imgui that we're starting a new frame

            int frameIndex = wrpRenderer.getFrameIndex();
            FrameInfo frameInfo {frameIndex, frameTime, commandBuffer, camera,
                globalDescriptorSets[frameIndex], sceneObjects};

            // UPDATE SECTION
            // Обновление данных внутри uniform buffer объектов для текущего кадра
            GlobalUbo ubo{};
            ubo.projection = camera.getProjection();
            ubo.view = camera.getView();
            ubo.inverseView = camera.getInverseView();
            pointLightSystem.update(frameInfo, ubo);
            uboBuffers[frameIndex]->writeToBuffer(&ubo);
            uboBuffers[frameIndex]->flush();
            TextureSystemUbo textureSystemUbo{};
            textureSystemUbo.directionalLightIntensity = wrpImgui.directionalLightIntensity;
            textureSystemUbo.directionalLightPosition = wrpImgui.directionalLightPosition;
            textureRenderSystem.update(frameInfo, textureSystemUbo);

            // RENDER SECTION
            /* Начало и конец прохода рендера и кадра отделены друг от друга для упрощения в дальнейшем
               интеграции сразу нескольких проходов рендера (Render passes) для создания отражений,
               теней и эффектов пост-процесса. */
            wrpRenderer.beginSwapChainRenderPass(commandBuffer, wrpImgui.clear_color);

            // Порядок отрисовки объектов важен, так как сначала надо отрисовать непрозрачные объекты с помощью textureRenderSystem, а
            // затем полупрозрачные билборды поинт лайтов с помощью PointLightSystem.
            simpleRenderSystem.renderGameObjects(frameInfo);
            textureRenderSystem.renderGameObjects(frameInfo);
            pointLightSystem.render(frameInfo);

            // Описание элементов интерфейса ImGUI для отрисовки
            wrpImgui.runExample();
            wrpImgui.showPointLightCreator();
            wrpImgui.showModelsFromDirectory();
            wrpImgui.enumerateObjectsInTheScene();
            wrpImgui.render(commandBuffer); // as last step in render pass, record the imgui draw commands

            wrpRenderer.endSwapChainRenderPass(commandBuffer);
            wrpRenderer.endFrame();
        }
    }

    vkDeviceWaitIdle(wrpDevice.device());  // ожидать завершения всех операций на GPU перед закрытием программы и очисткой всех ресурсов
}

void App::loadSceneObjects()
{
    // Viking Room model
   /* std::shared_ptr<WrpModel> vikingRoom = WrpModel::createModelFromObjTexture(
        wrpDevice, ENGINE_DIR"models/viking_room.obj", MODELS_DIR"textures/viking_room.png");
    auto vikingRoomObj = WrpGameObject::createGameObject();
    vikingRoomObj.model = vikingRoom;
    vikingRoomObj.transform.translation = {.0f, .0f, 0.f};
    vikingRoomObj.transform.scale = glm::vec3(1.f, 1.f, 1.f);
    vikingRoomObj.transform.rotation = glm::vec3(1.57f, 2.f, 0.f);
    sceneObjects.emplace(vikingRoomObj.getId(), std::move(vikingRoomObj));*/

    // Sponza model
    /*std::shared_ptr<WrpModel> sponza = WrpModel::createModelFromObjMtl(wrpDevice, "../../../models/sponza.obj");
    auto sponzaObj = WrpGameObject::createGameObject("Sponza");
    sponzaObj.model = sponza;
    sponzaObj.transform.translation = {-3.f, 1.0f, -2.f};
    sponzaObj.transform.scale = glm::vec3(0.01f, 0.01f, 0.01f);
    sponzaObj.transform.rotation = glm::vec3(3.15f, 0.f, 0.f);
    sceneObjects.emplace(sponzaObj.getId(), std::move(sponzaObj));*/
}

ENGINE_END

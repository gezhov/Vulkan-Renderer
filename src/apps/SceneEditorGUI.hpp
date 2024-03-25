#pragma once

#include "../src/renderer/Device.hpp"
#include "../src/renderer/Window.hpp"
#include "../src/renderer/SceneObject.hpp"
#include "../src/renderer/Camera.hpp"
#include "./common/KeyboardMovementController.hpp"
#include "../src/renderer/FrameInfo.hpp"

// libs
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <ImGuizmo.h>

// std
#include <stdexcept>
#include <string>

static void check_vk_result(VkResult err) {
    if (err == 0) return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0) abort();
}

class SceneEditorGUI {
public:
    SceneEditorGUI(WrpWindow& window, WrpDevice& device, VkRenderPass renderPass,
        uint32_t imageCount, WrpCamera& camera, KeyboardMovementController& kmc,
        SceneObject::Map& sceneObjects, RenderingSettings& renderingSettings);
    ~SceneEditorGUI();

    SceneEditorGUI() = default;
    SceneEditorGUI& operator=(SceneEditorGUI& imgui) { return imgui; }

    void newFrame();
    void setupGUI();
    void render(VkCommandBuffer commandBuffer);

    // Fields controlled by tools
    float directionalLightIntensity = 1.0f;
    glm::vec4 directionalLightPosition = { 1.0f, -3.0f, -1.0f, 1.f };
    int reflectionModel = 1;
    ImVec4 clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    std::vector<std::string> objectsPaths;
    std::vector<std::string> objectsNames;
    std::string selectedObjPath = "";

    float pointLightIntensity = .0f;
    float pointLightRadius = .0f;
    glm::vec3 pointLightColor{1, 1, 1};

private:
    void setupAllWindows();
    void setupMainSettingsPanel();    // presented as "Vulkan Renderer" window
    void setupObjectCreationPanel();
    void showPointLightCreator();
    void showModelsFromDirectory();
    void enumerateObjectsInTheScene();
    void inspectObject(SceneObject& object, bool isPointLight);
    void renderTransformGizmo(TransformComponent& transform);

    bool showImGuiDemoWindow = false; // controllable by UI checkbox

    WrpDevice& wrpDevice;
    WrpCamera& camera;
    KeyboardMovementController& kmc;
    SceneObject::Map& sceneObjects;
    RenderingSettings& renderingSettings;

    VkDescriptorPool descriptorPool; // ImGui's descriptor pool
};

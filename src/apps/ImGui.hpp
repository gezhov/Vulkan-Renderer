#pragma once

#include "../src/renderer/Device.hpp"
#include "../src/renderer/Window.hpp"
#include "../src/renderer/SceneObject.hpp"
#include "../src/renderer/Camera.hpp"
#include "../src/renderer/KeyboardMovementController.hpp"

// libs
#include <imgui.h>
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

// This whole class is only necessary right now because it needs to manage the descriptor pool
// because we haven't set one up anywhere else in the application, and we manage the
// example state, otherwise all the functions could just be static helper functions if you prefered
class WrpImgui {
public:
    WrpImgui(WrpWindow& window, WrpDevice& device, VkRenderPass renderPass,
        uint32_t imageCount, WrpCamera& camera, KeyboardMovementController& kmc, SceneObject::Map& sceneObjects);
    ~WrpImgui();

    WrpImgui() = default;
    WrpImgui& operator=(WrpImgui& imgui) { return imgui; }

    void newFrame();

    void render(VkCommandBuffer commandBuffer);

    // Example state
    bool show_demo_window = false;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    void runExample();
    void showPointLightCreator();
    void showModelsFromDirectory();
    void enumerateObjectsInTheScene();
    void inspectObject(SceneObject& object, bool isPointLight);
    void renderTransformGizmo(TransformComponent& transform);

    // data
    float directionalLightIntensity = 1.0f;
    glm::vec4 directionalLightPosition = { 1.0f, -3.0f, -1.0f, 1.f };

    std::vector<std::string> objectsPaths;
    std::string selectedObjPath = "";

    float pointLightIntensity = .0f;
    float pointLightRadius = .0f;
    glm::vec3 pointLightColor{};

private:
    WrpDevice& wrpDevice;
    WrpCamera& camera;
    KeyboardMovementController& kmc;
    SceneObject::Map& sceneObjects;

    VkDescriptorPool descriptorPool; // ImGui's descriptor pool
};

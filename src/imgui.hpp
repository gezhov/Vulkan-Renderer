#pragma once

#include "device.hpp"
#include "window.hpp"
#include "game_object.hpp"
#include "camera.hpp"
#include "keyboard_movement_controller.hpp"

// libs
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <ImGuizmo.h>

// std
#include <stdexcept>
#include <string>

// This whole class is only necessary right now because it needs to manage the descriptor pool
// because we haven't set one up anywhere else in the application, and we manage the
// example state, otherwise all the functions could just be static helper functions if you prefered
namespace vget {

    static void check_vk_result(VkResult err) {
        if (err == 0) return;
        fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
        if (err < 0) abort();
    }

    class VgetImgui {
    public:
        VgetImgui(VgetWindow& window, VgetDevice& device, VkRenderPass renderPass,
            uint32_t imageCount, VgetCamera& camera, KeyboardMovementController& kmc, VgetGameObject::Map& gameObjects);
        ~VgetImgui();

        VgetImgui() = default;
        VgetImgui& operator=(VgetImgui& imgui) { return imgui; }

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
        void inspectObject(VgetGameObject& object, bool isPointLight);
        void renderTransformGizmo(TransformComponent& transform);

        // data
        float directionalLightIntensity = .0f;
        glm::vec4 directionalLightPosition = { 1.0f, -3.0f, -1.0f, 1.f };

        std::vector<std::string> objectsPaths;
        std::string selectedObjPath = "";

        float pointLightIntensity = .0f;
        float pointLightRadius = .0f;
        glm::vec3 pointLightColor{};

    private:
        VgetDevice& vgetDevice;
        VgetCamera& camera;
        KeyboardMovementController& kmc;
        VgetGameObject::Map& gameObjects;

        VkDescriptorPool descriptorPool; // ImGui's descriptor pool
    };
}  // namespace lve

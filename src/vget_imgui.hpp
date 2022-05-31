#pragma once

#include "vget_device.hpp"
#include "vget_window.hpp"
#include "vget_game_object.hpp"
#include "vget_camera.hpp"

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
			uint32_t imageCount, VgetCamera& camera, VgetGameObject::Map& gameObjects);
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
		void enumerateObjectsInTheScene(VgetGameObject::Map& objects);
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
		VgetGameObject::Map& gameObjects;

		// We haven't yet covered descriptor pools in the tutorial series
		// so I'm just going to create one for just imgui and store it here for now.
		// maybe its preferred to have a separate descriptor pool for imgui anyway,
		// I haven't looked into imgui best practices at all.
		VkDescriptorPool descriptorPool;
	};
}  // namespace lve

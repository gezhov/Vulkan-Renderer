#include "vget_imgui.hpp"

#include "vget_device.hpp"
#include "vget_window.hpp"

// libs
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#define GLM_FORCE_RADIANS			  // Функции GLM будут работать с радианами, а не градусами
#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // GLM будет ожидать интервал нашего буфера глубины от 0 до 1 (например, для OpenGL используется интервал от -1 до 1)
#include <glm/gtc/type_ptr.hpp>

// std
#include <stdexcept>

namespace vget {

    // ok this just initializes imgui using the provided integration files. So in our case we need to
    // initialize the vulkan and glfw imgui implementations, since that's what our engine is built
    // using.
    VgetImgui::VgetImgui(
        VgetWindow& window, VgetDevice& device, VkRenderPass renderPass, uint32_t imageCount, VgetCamera& camera)
        : vgetDevice{ device }, camera{ camera } {
        // set up a descriptor pool stored on this instance, see header for more comments on this.
        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000} };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        if (vkCreateDescriptorPool(device.device(), &pool_info, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up descriptor pool for imgui");
        }

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

        // Setup Dear ImGui style
        //ImGui::StyleColorsDark();
         ImGui::StyleColorsClassic();

        // Setup Platform/Renderer backends
        // Initialize imgui for vulkan
        ImGui_ImplGlfw_InitForVulkan(window.getGLFWwindow(), true);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = device.getInstance();
        init_info.PhysicalDevice = device.getPhysicalDevice();
        init_info.Device = device.device();
        init_info.QueueFamily = device.getGraphicsQueueFamily();
        init_info.Queue = device.graphicsQueue();

        // pipeline cache is a potential future optimization, ignoring for now
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = descriptorPool;
        // todo, I should probably get around to integrating a memory allocator library such as Vulkan
        // memory allocator (VMA) sooner than later. We don't want to have to update adding an allocator
        // in a ton of locations.
        init_info.Allocator = VK_NULL_HANDLE;
        init_info.MinImageCount = 2;
        init_info.ImageCount = imageCount;
        init_info.CheckVkResultFn = check_vk_result;
        ImGui_ImplVulkan_Init(&init_info, renderPass);

        // upload fonts, this is done by recording and submitting a one time use command buffer
        // which can be done easily bye using some existing helper functions on the lve device object
        auto commandBuffer = device.beginSingleTimeCommands();
        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
        device.endSingleTimeCommands(commandBuffer);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    VgetImgui::~VgetImgui() {
        vkDestroyDescriptorPool(vgetDevice.device(), descriptorPool, nullptr);
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void VgetImgui::newFrame() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    // this tells imgui that we're done setting up the current frame,
    // then gets the draw data from imgui and uses it to record to the provided
    // command buffer the necessary draw commands
    void VgetImgui::render(VkCommandBuffer commandBuffer) {
        ImGui::Render();
        ImDrawData* drawdata = ImGui::GetDrawData();
        ImGui_ImplVulkan_RenderDrawData(drawdata, commandBuffer);
    }

    void VgetImgui::runExample() {
        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can
        // browse its code to learn more about Dear ImGui!).
        if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named
        // window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");  // Create a window called "Hello, world!" and append into it.

            ImGui::Text(
                "This is some useful text.");  // Display some text (you can use a format strings too)
            ImGui::Checkbox(
                "Demo Window",
                &show_demo_window);  // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float: Directional Light intensity", &directionalLightIntensity, -1.0f, 1.0f);  // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color",
                (float*)&clear_color);  // Edit 3 floats representing a color

            if (ImGui::Button("Button"))  // Buttons return true when clicked (most widgets return true
                                          // when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text(
                "Application average %.3f ms/frame (%.1f FPS)",
                1000.0f / ImGui::GetIO().Framerate,
                ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window) {
            ImGui::Begin(
                "Another Window",
                &show_another_window);  // Pass a pointer to our bool variable (the window will have a
                                        // closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me")) show_another_window = false;
            ImGui::End();
        }
    }

    void VgetImgui::inspectObject(VgetGameObject& object) {
        if (ImGui::Begin("Inspector")) {
            /*if (Scene::selectedEntity != nullptr) {
                Scene::InspectEntity(Scene::selectedEntity);
            }*/

            //ImGui::InputText("Name", &entity->name);
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::DragFloat3("Position", glm::value_ptr(object.transform.translation));
                ImGui::DragFloat3("Scale", glm::value_ptr(object.transform.scale));
                ImGui::DragFloat3("Rotation", glm::value_ptr(object.transform.rotation));
                renderTransformGizmo(object.transform);
            }
            /*if (entity->entityType == EntityType::Model) {
                InspectModel((Model*)entity);
            }
            else if (entity->entityType == EntityType::Light) {
                InspectLight((Light*)entity);
            }*/
        }
        ImGui::End();
    }

    void VgetImgui::renderTransformGizmo(TransformComponent& transform) {
        ImGuizmo::BeginFrame();
        static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::ROTATE;
        static ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;

        if (ImGui::IsKeyPressed(GLFW_KEY_1)) {
            currentGizmoOperation = ImGuizmo::TRANSLATE;
        }
        if (ImGui::IsKeyPressed(GLFW_KEY_2)) {
            currentGizmoOperation = ImGuizmo::ROTATE;
        }
        if (ImGui::IsKeyPressed(GLFW_KEY_3)) {
            currentGizmoOperation = ImGuizmo::SCALE;
        }
        if (ImGui::RadioButton("Translate", currentGizmoOperation == ImGuizmo::TRANSLATE)) {
            currentGizmoOperation = ImGuizmo::TRANSLATE;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate", currentGizmoOperation == ImGuizmo::ROTATE)) {
            currentGizmoOperation = ImGuizmo::ROTATE;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale", currentGizmoOperation == ImGuizmo::SCALE)) {
            currentGizmoOperation = ImGuizmo::SCALE;
        }

        if (currentGizmoOperation != ImGuizmo::SCALE) {
            if (ImGui::RadioButton("Local", currentGizmoMode == ImGuizmo::LOCAL)) {
                currentGizmoMode = ImGuizmo::LOCAL;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("World", currentGizmoMode == ImGuizmo::WORLD)) {
                currentGizmoMode = ImGuizmo::WORLD;
            }
        }
        else {
            currentGizmoMode = ImGuizmo::LOCAL;
        }
        glm::mat4 modelMat = transform.mat4();
        glm::mat4 guizmoProj(camera.getProjection());
        guizmoProj[1][1] *= -1;

        ImGuiIO& io = ImGui::GetIO();
        ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
        ImGuizmo::Manipulate(glm::value_ptr(camera.getView()), glm::value_ptr(guizmoProj), currentGizmoOperation,
            currentGizmoMode, glm::value_ptr(modelMat), nullptr, nullptr);

        /*if (transform.parent != nullptr) {
            modelMat = glm::inverse(transform.parent->GetMatrix()) * modelMat;
        }
        transform.transform = modelMat;*/

        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(modelMat), glm::value_ptr(transform.translation),
            glm::value_ptr(transform.rotation), glm::value_ptr(transform.scale));

        // Преобразование градусов в радианы.
        // todo: поворот дёргается. нужен фикс
        transform.rotation = glm::radians(transform.rotation);
    }
}  // vget

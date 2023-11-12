#include "SceneEditorGUI.hpp"

#include "../src/renderer/Device.hpp"
#include "../src/renderer/Window.hpp"

// libs
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#define GLM_FORCE_RADIANS			  // Функции GLM будут работать с радианами, а не градусами
#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // GLM будет ожидать интервал нашего буфера глубины от 0 до 1 (например, для OpenGL используется интервал от -1 до 1)
#include <glm/gtc/type_ptr.hpp>

// std
#include <stdexcept>
#include <fstream>
#include <filesystem>

SceneEditorGUI::SceneEditorGUI(
    WrpWindow& window, WrpDevice& device, VkRenderPass renderPass,
    uint32_t imageCount, WrpCamera& camera, KeyboardMovementController& kmc, SceneObject::Map& sceneObjects)
    : wrpDevice{ device }, camera{ camera }, kmc{ kmc }, sceneObjects{ sceneObjects }
{
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
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    //ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    // Initialize imgui for vulkan
    ImGui_ImplGlfw_InitForVulkan(window.getGLFWwindow(), true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = device.getInstance();
    init_info.PhysicalDevice = device.getPhysicalDevice();
    init_info.Device = device.device();
    init_info.QueueFamily = device.getGraphicsQueueFamily();
    init_info.Queue = device.graphicsQueue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptorPool;
    init_info.Allocator = VK_NULL_HANDLE;
    init_info.MinImageCount = 2;
    init_info.ImageCount = imageCount;
    init_info.MSAASamples = wrpDevice.getMaxUsableMSAASampleCount();
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info, renderPass);

    // Upload fonts. This is done by recording and submitting a one time use command buffer
    // which can be done easily by using some existing helper functions on the device object.
    auto commandBuffer = device.beginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
    device.endSingleTimeCommands(commandBuffer);
    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

SceneEditorGUI::~SceneEditorGUI()
{
    vkDestroyDescriptorPool(wrpDevice.device(), descriptorPool, nullptr);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void SceneEditorGUI::newFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

// this tells imgui that we're done setting up the current frame,
// then gets the draw data from imgui and uses it to record to the provided
// command buffer the necessary draw commands
void SceneEditorGUI::render(VkCommandBuffer commandBuffer)
{
    ImGui::Render();
    ImDrawData* drawdata = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(drawdata, commandBuffer);
}

void SceneEditorGUI::setupGUI()
{
    // this function may include DockSpace layout creation in the future
    setupAllWindows(); // all tools windows that integrated in the DockSpace
}

void SceneEditorGUI::setupAllWindows()
{
    // Show the demo ImGui window (browse its code for better understanding of functionality)
    if (show_demo_window) { ImGui::ShowDemoWindow(&show_demo_window); }

    setupSceneControlPanel();
    enumerateObjectsInTheScene();
    showPointLightCreator();
    showModelsFromDirectory();
}

void SceneEditorGUI::setupSceneControlPanel()
{
    ImGui::SetNextWindowPos(ImVec2{.0f, .0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{390, 250}, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Scene Control Panel")) {
        ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.9f);

        ImGui::Text("Demo window for further investigation:");
        ImGui::Checkbox("Show Demo Window", &show_demo_window);

        ImGui::Text("Directional Light intensity");
        ImGui::SliderFloat("##Directional Light intensity", &directionalLightIntensity, -1.0f, 1.0f);

        ImGui::Text("Directional Light Position");
        ImGui::DragFloat4("##Directional Light Position", glm::value_ptr(directionalLightPosition), .02f);

        ImGui::Text("Clear Color");
        ImGui::ColorEdit3("##Clear Color", (float*)&clearColor);

        ImGui::PopItemWidth();
        ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
        ImGui::Text("Camera Move and Rotate Speed");
        ImGui::DragFloat("##MoveSpeed", &kmc.moveSpeed, .01f);
        ImGui::SameLine();
        ImGui::DragFloat("##RotateSpeed", &kmc.lookSpeed, .01f);

        ImGui::Text(
            "Application average %.3f ms/frame (%.1f FPS)",
            1000.0f / ImGui::GetIO().Framerate,
            ImGui::GetIO().Framerate);

        ImGui::PopItemWidth();
    }
    ImGui::End();
}

void SceneEditorGUI::enumerateObjectsInTheScene()
{
    ImGui::SetNextWindowPos(ImVec2{0, 255}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{250, 230}, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("All Objects")) {
        static int pickedItemId = 0; // first item is the default one to pick
        if (ImGui::BeginListBox("All Objects", ImVec2(-FLT_MIN, 10 * ImGui::GetTextLineHeightWithSpacing())))
        {
            for (auto& obj : sceneObjects)
            {
                const bool isSelected = (pickedItemId == obj.second.getId());
                if (ImGui::Selectable(obj.second.getName().c_str(), isSelected)) {
                    pickedItemId = obj.second.getId();
                }

                if (isSelected) { ImGui::SetItemDefaultFocus(); }
            }
            ImGui::EndListBox();
        }

        // Create "Inspect Object" window for chosed type of scene object
        if (sceneObjects[pickedItemId].pointLight != nullptr) {
            inspectObject(sceneObjects[pickedItemId], true);
        }
        else {
            inspectObject(sceneObjects[pickedItemId], false);
        }
    }
    ImGui::End();
}

void SceneEditorGUI::inspectObject(SceneObject& object, bool isPointLight)
{
    ImGui::SetNextWindowPos(ImVec2{0, 490}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{350, 290}, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Inspector")) {
        if (ImGui::CollapsingHeader("Transform Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Position", glm::value_ptr(object.transform.translation), 0.02f);
            ImGui::DragFloat3("Scale", glm::value_ptr(object.transform.scale), 0.02f);
            ImGui::DragFloat3("Rotation", glm::value_ptr(object.transform.rotation), 0.02f);
        }

        renderTransformGizmo(object.transform); // render object's gizmo along with its inspector tool

        if (isPointLight) {
            if (ImGui::CollapsingHeader("PointLight Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderFloat("Light intensity", &object.pointLight->lightIntensity, .0f, 500.0f);
                ImGui::SliderFloat("Light radius", &object.transform.scale.x, 0.01f, 10.0f);
                ImGui::ColorEdit3("Light color", (float*)&object.color);
                ImGui::Checkbox("Demo Carousel Enabled", &object.pointLight->carouselEnabled);
            }
        }
    }
    ImGui::End();
}

void SceneEditorGUI::showModelsFromDirectory()
{
    std::string path(MODELS_DIR);
    std::string ext(".obj");
    objectsPaths.clear();
    for (auto& p : std::filesystem::recursive_directory_iterator(path))
    {
        if (p.path().extension() == ext)
        {
            objectsPaths.push_back(p.path().string());
        }
    }

    ImGui::SetNextWindowPos(ImVec2{395, 0}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{300, 300}, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Object Loader")) {
        static int pickedItemId = 0; // В статической переменной функции хранится номер выбранного из списка айтема
        ImGui::Text("Available models to add to the scene:");
        ImGui::Text(selectedObjPath.c_str());
        if (ImGui::BeginListBox("Object Loader", ImVec2(-FLT_MIN, 12 * ImGui::GetTextLineHeightWithSpacing())))
        {
            for (int n = 0; n < objectsPaths.size(); n++)
            {
                const bool isSelected = (pickedItemId == n);
                // Список заполняется элементами на основе переданных строк
                if (ImGui::Selectable(objectsPaths[n].c_str(), isSelected)) {
                    pickedItemId = n;
                    selectedObjPath = objectsPaths.at(n);
                }

                // Установить фокус на выбранный айтем
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndListBox();
        }

        if (ImGui::Button("Add to the scene")) {
            std::shared_ptr<WrpModel> model = WrpModel::createModelFromObjMtl(wrpDevice, objectsPaths.at(pickedItemId));
            auto newObj = SceneObject::createSceneObject();
            newObj.model = model;
            sceneObjects.emplace(newObj.getId(), std::move(newObj));
        }
    }
    ImGui::End();
}

void SceneEditorGUI::showPointLightCreator()
{
    ImGui::SetNextWindowPos(ImVec2{395, 305}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{300, 180}, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Point Light Creator"))
    {
        ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.9f);

        ImGui::Text("Intensity");
        ImGui::SliderFloat("##Point Light intensity", &pointLightIntensity, .0f, 500.0f);

        ImGui::Text("Radius");
        ImGui::SliderFloat("##Point Light radius", &pointLightRadius, 0.01f, 10.0f);

        ImGui::Text("Point Light Color");
        ImGui::ColorEdit3("##Point Light color", (float*)&pointLightColor);

        if (ImGui::Button("Add Point Light"))
        {
            SceneObject pointLight = SceneObject::makePointLight(pointLightIntensity, pointLightRadius, pointLightColor);
            sceneObjects.emplace(pointLight.getId(), std::move(pointLight));
        }

        ImGui::PopItemWidth();
    }
    ImGui::End();
}

void SceneEditorGUI::renderTransformGizmo(TransformComponent& transform)
{
    ImGuizmo::BeginFrame();
    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::ROTATE;
    static ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;

    if (ImGui::IsKeyPressed(ImGuiKey_1)) {
        currentGizmoOperation = ImGuizmo::TRANSLATE;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_2)) {
        currentGizmoOperation = ImGuizmo::ROTATE;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_3)) {
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
    glm::mat4 deltaMat{};
    glm::mat4 guizmoProj(camera.getProjection());
    guizmoProj[1][1] *= -1;

    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    ImGuizmo::Manipulate(glm::value_ptr(camera.getView()), glm::value_ptr(guizmoProj), currentGizmoOperation,
        currentGizmoMode, glm::value_ptr(modelMat), glm::value_ptr(deltaMat), nullptr);

    /*if (transform.parent != nullptr) {
        modelMat = glm::inverse(transform.parent->GetMatrix()) * modelMat;
    }
    transform.transform = modelMat;*/

    /*glm::vec3 deltaTranslation{};
    glm::vec3 deltaRotation{};
    glm::vec3 deltaScale{};*/
    /*ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(deltaMat), glm::value_ptr(deltaTranslation),
        glm::value_ptr(deltaRotation), glm::value_ptr(deltaScale));*/

    glm::vec3 empty{};

    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(modelMat), glm::value_ptr(transform.translation),
        glm::value_ptr(empty), glm::value_ptr(transform.scale));

    // Преобразование градусов в радианы.
    // todo: поворот дёргается. нужен фикс
    //transform.rotation = glm::radians(transform.rotation);
}

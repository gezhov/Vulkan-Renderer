#include "SceneEditorGUI.hpp"

#include "../src/renderer/Device.hpp"
#include "../src/renderer/Window.hpp"

// libs
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/type_ptr.hpp>

// std
#include <stdexcept>
#include <fstream>
#include <filesystem>

SceneEditorGUI::SceneEditorGUI(
    WrpWindow& window, WrpDevice& device, VkRenderPass renderPass,
    uint32_t imageCount, WrpCamera& camera, KeyboardMovementController& kmc,
    SceneObject::Map& sceneObjects, RenderingSettings& renderingSettings)
    : wrpDevice{device}, camera{camera}, kmc{kmc}, sceneObjects{sceneObjects},
    renderingSettings{renderingSettings}
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
        throw std::runtime_error("Failed to set up descriptor pool for imgui.");
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
    init_info.DescriptorPool = descriptorPool;
    init_info.RenderPass = renderPass;
    init_info.MinImageCount = 2;
    init_info.ImageCount = imageCount;
    init_info.MSAASamples = wrpDevice.getMaxUsableMSAASampleCount();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.Allocator = VK_NULL_HANDLE;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_CreateFontsTexture();
}

SceneEditorGUI::~SceneEditorGUI()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(wrpDevice.device(), descriptorPool, nullptr);
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
    setupAllWindows();
}

void SceneEditorGUI::setupAllWindows()
{
    // Show the demo ImGui window (browse its code for better understanding of functionality)
    if (showImGuiDemoWindow) { ImGui::ShowDemoWindow(&showImGuiDemoWindow); }

    setupMainSettingsPanel();
    enumerateObjectsInTheScene();
    setupObjectCreationPanel();
}

void SceneEditorGUI::setupMainSettingsPanel()
{
    ImGui::SetNextWindowPos(ImVec2{.0f, .0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{390, 270}, ImGuiCond_FirstUseEver);

    // window itself
    if (ImGui::Begin("Vulkan Renderer"))
    {
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
            1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

        // 1 collapsing header
        if (ImGui::CollapsingHeader("Scene Rendering Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.95f);
            ImGui::Text("Directional Light Intensity");
            ImGui::SliderFloat("##Directional Light intensity", &directionalLightIntensity, -1.0f, 10.0f);

            ImGui::Text("Directional Light Position");
            ImGui::DragFloat4("##Directional Light Position", glm::value_ptr(directionalLightPosition), .02f);

            ImGui::Text("Reflection Model");
            ImGui::RadioButton("Lambertian", &renderingSettings.reflectionModel, 0); ImGui::SameLine();
            ImGui::RadioButton("Blinn-Phong", &renderingSettings.reflectionModel, 1); ImGui::SameLine();
            ImGui::RadioButton("Torrance-Sparrow", &renderingSettings.reflectionModel, 2);
            ImGui::PopItemWidth();

            if (renderingSettings.reflectionModel == 2) {
                ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.6f);
                ImGui::Text("Torrance-Sparrow Model settings");
                ImGui::SliderFloat("Diffuse Proportion", &diffuseProportion, 0.f, 1.f);
                ImGui::SliderFloat("Roughness (C3)", &roughness, 0.f, 1.f);
                ImGui::SliderFloat("Index of Refraction (n)", &indexOfRefraction, 0.1f, 300.f);
                ImGui::PopItemWidth();
            }

            ImGui::Text("Polygon Fill Mode");
            ImGui::RadioButton("Fill", &renderingSettings.polygonFillMode, 0); ImGui::SameLine();
            ImGui::RadioButton("Wireframe", &renderingSettings.polygonFillMode, 1); ImGui::SameLine();
            ImGui::RadioButton("Point", &renderingSettings.polygonFillMode, 2);

            ImGui::Text("Clear Color");
            ImGui::ColorEdit3("##Clear Color", (float*)&clearColor);
        }

        // 2 collapsing header
        ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
        if (ImGui::CollapsingHeader("Camera Controller Settings"))
        {
            ImGui::Text("Camera Move and Rotate Speed");
            ImGui::DragFloat("##MoveSpeed", &kmc.moveSpeed, .01f);
            ImGui::SameLine();
            ImGui::DragFloat("##RotateSpeed", &kmc.lookSpeed, .01f);
        }
        ImGui::PopItemWidth();

        ImGui::Separator();
        ImGui::Checkbox("Show ImGui Demo Window", &showImGuiDemoWindow);
    }
    ImGui::End();
}

void SceneEditorGUI::enumerateObjectsInTheScene()
{
    ImGui::SetNextWindowPos(ImVec2{0, 275}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{200, 230}, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("All Objects")) {
        if (ImGui::BeginListBox("All Objects", ImVec2(-FLT_MIN, 10 * ImGui::GetTextLineHeightWithSpacing())))
        {
            for (auto& obj : sceneObjects)
            {
                const bool isSelected = (pickedItemSceneObjectsList == obj.second.getId());
                if (ImGui::Selectable(obj.second.getName().c_str(), isSelected)) {
                    pickedItemSceneObjectsList = obj.second.getId();
                }

                if (isSelected) { ImGui::SetItemDefaultFocus(); }
            }
            ImGui::EndListBox();
        }

        // Create "Inspect Object" window for chosed type of scene object
        if (sceneObjects[pickedItemSceneObjectsList].pointLight != nullptr) {
            inspectObject(sceneObjects[pickedItemSceneObjectsList], true);
        }
        else {
            inspectObject(sceneObjects[pickedItemSceneObjectsList], false);
        }
    }
    ImGui::End();
}

void SceneEditorGUI::inspectObject(SceneObject& object, bool isPointLight)
{
    ImGui::SetNextWindowPos(ImVec2{0, 510}, ImGuiCond_FirstUseEver);
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
                ImGui::SliderFloat("Light intensity", &object.pointLight->lightIntensity, .0f, 100.0f);
                ImGui::SliderFloat("Light radius", &object.transform.scale.x, 0.01f, 5.0f);
                ImGui::ColorEdit3("Light color", (float*)&object.color);
                ImGui::Checkbox("Demo Carousel Enabled", &object.pointLight->carouselEnabled);
            }
        }
    }
    ImGui::End();
}

void SceneEditorGUI::setupObjectCreationPanel()
{
    ImGui::SetNextWindowPos(ImVec2{395, 0}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{300, 330}, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Object Creator"))
    {
        if (ImGui::BeginTabBar("TabBar"))
        {
            if (ImGui::BeginTabItem("Model Loader"))
            {
                showModelsFromDirectory();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Point Light Creator"))
            {
                showPointLightCreator();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void SceneEditorGUI::showModelsFromDirectory()
{
    std::string path(MODELS_DIR);
    std::string ext(".obj");
    objectsPaths.clear();
    objectsNames.clear();
    for (auto& p : std::filesystem::recursive_directory_iterator(path))
    {
        if (p.path().extension() == ext)
        {
            // Names are shown in the list, paths are used for models loading
            objectsPaths.push_back(p.path().string());
            size_t pathStringSize = p.path().string().size();
            objectsNames.push_back(p.path().string().substr(16, pathStringSize));
        }
    }

    ImGui::Text("Available models to add to the scene:");
    ImGui::Text(selectedObjPath.c_str());
    if (ImGui::BeginListBox("Object Loader", ImVec2(-FLT_MIN, 12 * ImGui::GetTextLineHeightWithSpacing())))
    {
        for (int n = 0; n < objectsNames.size(); n++)
        {
            const bool isSelected = (pickedItemModelsList == n);
            // List are filling up with passed labels 
            if (ImGui::Selectable(objectsNames[n].c_str(), isSelected)) {
                pickedItemModelsList = n;
                selectedObjPath = objectsPaths.at(n);
            }

            // Focus on chosen item
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndListBox();
    }

    if (ImGui::Button("Add to the scene")) {
        std::shared_ptr<WrpModel> model = WrpModel::createModelFromObjMtl(wrpDevice, objectsPaths.at(pickedItemModelsList));
        auto newObj = SceneObject::createSceneObject();
        newObj.model = model;
        sceneObjects.emplace(newObj.getId(), std::move(newObj));
        pickedItemSceneObjectsList = newObj.getId();
    }
}

void SceneEditorGUI::showPointLightCreator()
{
    ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.9f);

    ImGui::Text("Intensity");
    ImGui::SliderFloat("##Point Light intensity", &pointLightIntensity, .0f, 100.0f);

    ImGui::Text("Radius");
    ImGui::SliderFloat("##Point Light radius", &pointLightRadius, 0.01f, 4.0f);

    ImGui::Text("Point Light Color");
    ImGui::ColorEdit3("##Point Light color", (float*)&pointLightColor);

    if (ImGui::Button("Add Point Light"))
    {
        SceneObject pointLight = SceneObject::makePointLight(pointLightIntensity, pointLightRadius, pointLightColor);
        sceneObjects.emplace(pointLight.getId(), std::move(pointLight));
        pickedItemSceneObjectsList = pointLight.getId();
    }

    ImGui::PopItemWidth();
}

void SceneEditorGUI::renderTransformGizmo(TransformComponent& transform)
{
    ImGuizmo::BeginFrame();
    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
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

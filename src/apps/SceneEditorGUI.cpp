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

// ok this just initializes imgui using the provided integration files. So in our case we need to
// initialize the vulkan and glfw imgui implementations, since that's what our engine is built
// using.
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
    ImGui::StyleColorsClassic();
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

    // pipeline cache is a potential future optimization, ignoring for now
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptorPool;
    // todo, I should probably get around to integrating a memory allocator library such as Vulkan
    // memory allocator (VMA) sooner than later. We don't want to have to update adding an allocator
    // in a ton of locations.
    init_info.Allocator = VK_NULL_HANDLE;
    init_info.MinImageCount = 2;
    init_info.ImageCount = imageCount;
    init_info.MSAASamples = wrpDevice.getMaxUsableMSAASampleCount();
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info, renderPass);

    // upload fonts, this is done by recording and submitting a one time use command buffer
    // which can be done easily by using some existing helper functions on the device object
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
    // Styling for all of windows
    // Title color seems like a not exact. I don't know why for now.
    // It can be tweaked just to imitate Vulkan original color and I need it to blink a bit when window active/non-acitve (also for tabs).
    // Also background colors of the window is changing to other values when they are docked.
    // Maybe it has something to do with the parent window from DockSpace, but I don't know exactly for now.
    ImGui::PushStyleColor(ImGuiCol_TitleBg, IM_COL32(172, 22, 44, 255));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, IM_COL32(172, 22, 44, 255));
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, IM_COL32(172, 22, 44, 255));
    ImGui::PushStyleColor(ImGuiCol_Tab, IM_COL32(172, 22, 44, 255));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, IM_COL32(172, 22, 44, 255));
    ImGui::PushStyleColor(ImGuiCol_TabActive, IM_COL32(172, 22, 44, 255));
    ImGui::PushStyleColor(ImGuiCol_TabUnfocused, IM_COL32(172, 22, 44, 255));
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, IM_COL32(172, 22, 44, 255));
#define STYLE_COLOR_NUM 8

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    // First created window ("DockSpaceRootWindow") used as container for the DockSpace
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    // Styling for the root window with DockSpace 
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_DockingEmptyBg, ImVec4(0.0f, 0.0, 0.0f, 0.0f));
    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;

    ImGui::Begin("DockSpaceRootWindow", NULL, windowFlags);
        ImGuiID dockspaceId = ImGui::GetID("RootDockSpace");
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
    ImGui::PopStyleVar(3);   // Disabling root window stylings
    ImGui::PopStyleColor();

    setupAllWindows(); // all windows used for the DockSpace

    ImGui::PopStyleColor(STYLE_COLOR_NUM); // disabling all remaining color stylings

    // TODO: create proper layout
    static float RATIO_1_5 = 0.8f;
    static float RATIO_5_1 = 0.2f;
    static float RATIO_1_4 = 0.25;

    static ImVec2 savedWindowSize = {};
    auto currentWindowSize = ImGui::GetWindowSize();

    // Rebuild DockSpace when resize has happened. Inspired by: https://github.com/ocornut/imgui/issues/6095
    if (currentWindowSize.x != savedWindowSize.x || currentWindowSize.y != savedWindowSize.y)
    {
        // I removed ImGuiDockNodeFlags_PassthruCentralNode from hear, because it somehow breaks Passthru for the RootDockSpace
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::DockBuilderSetNodeSize(dockspaceId, currentWindowSize);

        // splitting dockspace in down direction with given ratio
        ImGuiID topAreaId = -1;
        ImGuiID browserId = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Down, RATIO_1_4, nullptr, &topAreaId);
        // splitting topArea in left direction
        ImGuiID topRightAreaId = -1;
        ImGuiID hierarchyId = ImGui::DockBuilderSplitNode(topAreaId, ImGuiDir_Left, RATIO_5_1, nullptr, &topRightAreaId);
        // splitting topRightArea in left direction
        ImGuiID inspectorId = -1;
        ImGuiID viewId = ImGui::DockBuilderSplitNode(topRightAreaId, ImGuiDir_Left, RATIO_1_5, nullptr, &inspectorId);

        ImGui::DockBuilderDockWindow("Scene Control Panel", hierarchyId);
        ImGui::DockBuilderDockWindow("Inspector", inspectorId);
        ImGui::DockBuilderDockWindow("All Objects", viewId);
        ImGui::DockBuilderDockWindow("Point Light Creator", browserId);

        ImGui::DockBuilderFinish(dockspaceId);

        savedWindowSize = currentWindowSize;
    }
}

void SceneEditorGUI::setupAllWindows()
{

    // Show the demo ImGui window (browse its code for better understanding of functionality)
    if (show_demo_window) { ImGui::ShowDemoWindow(&show_demo_window); }

    setupSceneControlPanel();
    showPointLightCreator();
    showModelsFromDirectory();
    enumerateObjectsInTheScene();

}

void SceneEditorGUI::setupSceneControlPanel()
{

    ImGui::Begin("Scene Control Panel");
        ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.9f);

        ImGui::Text("Demo windows for further investigation:");  // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &show_demo_window);  // Edit bools storing our window open/close state

        ImGui::Text("Directional Light intensity");
        ImGui::SliderFloat("##Directional Light intensity", &directionalLightIntensity, -1.0f, 1.0f);

        ImGui::Text("Directional Light Position");
        ImGui::DragFloat4("##Directional Light Position", glm::value_ptr(directionalLightPosition), .02f);

        ImGui::Text("Clear Color");
        ImGui::ColorEdit3("##Clear Color", (float*)&clearColor);

        ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
        ImGui::Text("Camera Move and Rotate Speed");
        ImGui::DragFloat("##MoveSpeed", &kmc.moveSpeed, .01f);
        ImGui::SameLine();
        ImGui::DragFloat("##RotateSpeed", &kmc.lookSpeed, .01f);

        ImGui::Text(
            "Application average %.3f ms/frame (%.1f FPS)",
            1000.0f / ImGui::GetIO().Framerate,
            ImGui::GetIO().Framerate);
    ImGui::End();
}

void SceneEditorGUI::showPointLightCreator()
{
    if (!ImGui::Begin("Point Light Creator"))
    {
        ImGui::End();
        return;
    }
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

    if (ImGui::Begin("Object Loader")) {
        static int item_current_idx = 0; // В статической переменной функции хранится номер выбранного из списка айтема
        ImGui::Text("Available models to add to the scene:");
        ImGui::Text(selectedObjPath.c_str());
        if (ImGui::BeginListBox("Object Loader", ImVec2(-FLT_MIN, 12 * ImGui::GetTextLineHeightWithSpacing())))
        {
            for (int n = 0; n < objectsPaths.size(); n++)
            {
                const bool is_selected = (item_current_idx == n);
                // Список заполняется элементами на основе переданных строк
                if (ImGui::Selectable(objectsPaths[n].c_str(), is_selected)) {
                    item_current_idx = n;
                    selectedObjPath = objectsPaths.at(n);
                }

                // Установить фокус на выбранный айтем
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndListBox();
        }

        if (ImGui::Button("Add to the scene")) {
            std::shared_ptr<WrpModel> model = WrpModel::createModelFromObjMtl(wrpDevice, objectsPaths.at(item_current_idx));
            auto newObj = SceneObject::createSceneObject();
            newObj.model = model;
            sceneObjects.emplace(newObj.getId(), std::move(newObj));
        }
    }
    ImGui::End();
}

void SceneEditorGUI::enumerateObjectsInTheScene()
{
    if (ImGui::Begin("All Objects")) {
        static int item_current_idx = 0; // Здесь список подобен тому, что есть в Object Loader'е
        if (ImGui::BeginListBox("All Objects", ImVec2(-FLT_MIN, 10 * ImGui::GetTextLineHeightWithSpacing())))
        {
            for (auto& obj : sceneObjects)
            {
                const bool is_selected = (item_current_idx == obj.second.getId());
                if (ImGui::Selectable(obj.second.getName().c_str(), is_selected)) {
                    item_current_idx = obj.second.getId();
                }

                if (is_selected) { ImGui::SetItemDefaultFocus(); }
            }
            ImGui::EndListBox();
        }

        if (sceneObjects[item_current_idx].pointLight != nullptr) {
            inspectObject(sceneObjects[item_current_idx], true);
        }
        else {
            inspectObject(sceneObjects[item_current_idx], false);
        }
    }
    ImGui::End();
}

void SceneEditorGUI::inspectObject(SceneObject& object, bool isPointLight)
{
    if (ImGui::Begin("Inspector")) {
        /*if (Scene::selectedEntity != nullptr) {
            Scene::InspectEntity(Scene::selectedEntity);
        }*/
        if (ImGui::CollapsingHeader("Transform Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Position", glm::value_ptr(object.transform.translation), 0.02f);
            ImGui::DragFloat3("Scale", glm::value_ptr(object.transform.scale), 0.02f);
            ImGui::DragFloat3("Rotation", glm::value_ptr(object.transform.rotation), 0.02f);
        }
        renderTransformGizmo(object.transform);

        if (isPointLight) {
            if (ImGui::CollapsingHeader("PointLight Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderFloat("Light intensity", &object.pointLight->lightIntensity, .0f, 500.0f);
                ImGui::SliderFloat("Light radius", &object.transform.scale.x, 0.01f, 10.0f);
                ImGui::ColorEdit3("Light color", (float*)&object.color);
                ImGui::Checkbox("Demo Carousel Enabled", &object.pointLight->carouselEnabled);
            }
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

void SceneEditorGUI::renderTransformGizmo(TransformComponent& transform)
{
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

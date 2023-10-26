#pragma once

#include "../renderer/Window.hpp"
#include "../renderer/Device.hpp"
#include "../renderer/Renderer.hpp"
#include "../renderer/Descriptors.hpp"
#include "../renderer/SceneObject.hpp"

// std
#include <memory>
#include <vector>

class SceneEditorApp
{
public:
    static constexpr int WIDTH = 1600;
    static constexpr int HEIGHT = 1000;

    SceneEditorApp();
    ~SceneEditorApp();

    // RAII
    SceneEditorApp(const SceneEditorApp&) = delete;
    SceneEditorApp& operator=(const SceneEditorApp&) = delete;

    void run();

private:
    void loadSceneObjects();

    // Fields are initializing from top to bottom and destroying from bottom to top
    WrpWindow wrpWindow{ WIDTH, HEIGHT, "Vulkan Renderer" };
    WrpDevice wrpDevice{ wrpWindow };
    WrpRenderer wrpRenderer{ wrpWindow, wrpDevice };

    std::unique_ptr<WrpDescriptorPool> globalPool{};
    SceneObject::Map sceneObjects;
};

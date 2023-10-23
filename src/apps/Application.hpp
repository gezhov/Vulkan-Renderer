#pragma once

#include "../renderer/Window.hpp"
#include "../renderer/Device.hpp"
#include "../renderer/SceneObject.hpp"
#include "../renderer/Renderer.hpp"
#include "../renderer/Descriptors.hpp"

// std
#include <memory>
#include <vector>

ENGINE_BEGIN // todo : Target applications shoudn't be in the engine namespace i guess

class App
{
public:
    static constexpr int WIDTH = 1600;
    static constexpr int HEIGHT = 1000;

    App();
    ~App();

    // RAII
    App(const App&) = delete;
    App& operator=(const App&) = delete;

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

ENGINE_END

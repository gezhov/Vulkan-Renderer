#pragma once

#include "../renderer/Window.hpp"
#include "../renderer/Device.hpp"
#include "../renderer/Renderer.hpp"
#include "../renderer/Descriptors.hpp"
#include "../renderer/SceneObject.hpp"

// std
#include <memory>
#include <vector>

// Reflection Model Research App
class RMResearchApp
{
public:
    static constexpr int WIDTH = 1600;
    static constexpr int HEIGHT = 1000;

    RMResearchApp(int preloadScene = 0);
    ~RMResearchApp();

    RMResearchApp(const RMResearchApp&) = delete;
    RMResearchApp& operator=(const RMResearchApp&) = delete;

    void run();

private:
    void loadScene();

    WrpWindow wrpWindow{ WIDTH, HEIGHT, "Vulkan Renderer" };
    WrpDevice wrpDevice{ wrpWindow };
    WrpRenderer wrpRenderer{ wrpWindow, wrpDevice };

    std::unique_ptr<WrpDescriptorPool> globalPool{};
    SceneObject::Map sceneObjects;
};

#pragma once

#include "../renderer/window.hpp"
#include "../renderer/device.hpp"
#include "../renderer/game_object.hpp"
#include "../renderer/renderer.hpp"
#include "../renderer/descriptors.hpp"

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

    // Избавляемся от copy operator и copy constrcutor, т.к. App хранит в себе указатели
    // на VkPipelineLayout_T и VkCommandBuffer_T, которые лучше не копировать.
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    void run();

private:
    void loadGameObjects();

    // Порядок объявления перменных-членов имеет значение. Так, они будут инициализироваться
    // сверху вниз, а уничтожаться снизу вверх. Пул дескрипторов, таким образом, должен
    // быть объявлен после девайса.
    WrpWindow wrpWindown{ WIDTH, HEIGHT, "Vulkan Renderer" };
    WrpDevice wrpDevice{ wrpWindown };
    WrpRenderer wrpRenderer{ wrpWindown, wrpDevice };

    std::unique_ptr<WrpDescriptorPool> globalPool{};
    WrpGameObject::Map gameObjects;
};

ENGINE_END

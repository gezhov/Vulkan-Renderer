#pragma once

#include "hcore.hpp"
#include "window.hpp"
#include "device.hpp"
#include "game_object.hpp"
#include "renderer.hpp"
#include "descriptors.hpp"

// std
#include <memory>
#include <vector>

ENGINE_BEGIN // todo : FirstApp shoudn't be in engine namespace

class FirstApp
{
public:
    static constexpr int WIDTH = 1600;
    static constexpr int HEIGHT = 1000;

    FirstApp();
    ~FirstApp();

    // Избавляемся от copy operator и copy constrcutor, т.к. FirstApp хранит в себе указатели
    // на VkPipelineLayout_T и VkCommandBuffer_T, которые лучше не копировать.
    FirstApp(const FirstApp&) = delete;
    FirstApp& operator=(const FirstApp&) = delete;

    void run();

private:
    void loadGameObjects();

    // Порядок объявления перменных-членов имеет значение. Так, они будут инициализироваться
    // сверху вниз, а уничтожаться снизу вверх. Пул дескрипторов, таким образом, должен
    // быть объявлен после девайса.
    VgetWindow vgetWindow{ WIDTH, HEIGHT, "Vulkan Graphics Engine" };
    VgetDevice vgetDevice{ vgetWindow };
    VgetRenderer vgetRenderer{ vgetWindow, vgetDevice };

    std::unique_ptr<VgetDescriptorPool> globalPool{};
    VgetGameObject::Map gameObjects;
};

ENGINE_END

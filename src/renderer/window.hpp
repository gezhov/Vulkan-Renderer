#pragma once // Preprocessor directive to avoid multiple file inclusion.

#include "hcore.hpp"
#define GLFW_INCLUDE_VULKAN // GLFW will automatically include Vulkan headers for its work
#include <GLFW/glfw3.h>

#include <string>

ENGINE_BEGIN

class WrpWindow
{
public:
    WrpWindow(int w, int h, std::string name);
    ~WrpWindow();

    // Удаление copy constructor и copy operator, чтобы лишить объекты этого класса
    // возможности копирования. Это сделано, чтобы соблюсти принцип "resource acquisition is initialization",
    // что означает получение новых ресурсов только через отдельную инициализацию.
    // Такой подход позволяет избежать возможности получения нескольких объектов WrpWindow
    // с указателем на одно и то же окно GLFWwindow.
    WrpWindow(const WrpWindow&) = delete;
    WrpWindow& operator=(const WrpWindow&) = delete;

    bool shouldClose() { return glfwWindowShouldClose(window); }
    VkExtent2D getExtent() { return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) }; }
    bool wasWindowResized() { return framebufferResized; }
    void resetWindowsResizedFlag() { framebufferResized = false; }
    GLFWwindow* getGLFWwindow() const { return window; }

    void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);

private:
    // Инициализация библиотеки GLFW и окна
    void initWindow();
    // Callback функция, которая вызывается при изменении размера окна
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    int width;
    int height;
    bool framebufferResized = false;  // флаг изменения размера окна

    std::string windowName;
    GLFWwindow* window;
};

ENGINE_END

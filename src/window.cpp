#include "window.hpp"

// std
#include <stdexcept>

ENGINE_BEGIN

VgetWindow::VgetWindow(int w, int h, std::string name) : width{ w }, height{ h }, windowName{ name }
{
	initWindow();
}

VgetWindow::~VgetWindow()
{
	// Уничтожение окна и его контекста. Освобождение всех остальных ресурсов библиотеки GLFW.
	glfwDestroyWindow(window);
	glfwTerminate();
}

void VgetWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
{
	if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create window surface!");
	}
}

void VgetWindow::initWindow()
{
	glfwInit();  // GLFW library initialization

	// Настройки контекста окна GLFW перед его созданием.
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // Не создавать контекст графического API при создании окна.
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);     // Включить возможность изменять размер окна

	// Создание окна и его контекста.
	window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);  // Связывание указателя GLFWwindow* и указателя на текущий экземпляр VgetWindow*.
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);  // Установка callback функции на изменение размера окна (буфера кадра)
}

void VgetWindow::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	// Используя приведение reinterpret_cast(), получаем из указателя на окно GLFWwindow*
	// связанный с ним указатель на пользовательский тип окна VgetWindow*
	auto vgetWindow = reinterpret_cast<VgetWindow*>(glfwGetWindowUserPointer(window));

	vgetWindow->framebufferResized = true;
	vgetWindow->width = width;
	vgetWindow->height = height;
}

ENGINE_END

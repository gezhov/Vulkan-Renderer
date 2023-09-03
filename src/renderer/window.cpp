#include "window.hpp"

// std
#include <stdexcept>

ENGINE_BEGIN

WrpWindow::WrpWindow(int w, int h, std::string name) : width{ w }, height{ h }, windowName{ name }
{
	initWindow();
}

WrpWindow::~WrpWindow()
{
	// Уничтожение окна и его контекста. Освобождение всех остальных ресурсов библиотеки GLFW.
	glfwDestroyWindow(window);
	glfwTerminate();
}

void WrpWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
{
	if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create window surface!");
	}
}

void WrpWindow::initWindow()
{
	glfwInit();  // GLFW library initialization

	// Настройки контекста окна GLFW перед его созданием.
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // Не создавать контекст графического API при создании окна.
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);     // Включить возможность изменять размер окна

	// Создание окна и его контекста.
	window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);  // Связывание указателя GLFWwindow* и указателя на текущий экземпляр WrpWindow*.
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);  // Установка callback функции на изменение размера окна (буфера кадра)

    glfwMaximizeWindow(window); // максимизировать окно сразу после создания
}

void WrpWindow::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	// Используя приведение reinterpret_cast(), получаем из указателя на окно GLFWwindow*
	// связанный с ним указатель на пользовательский тип окна WrpWindow*
	auto wrpWindow = reinterpret_cast<WrpWindow*>(glfwGetWindowUserPointer(window));

	wrpWindow->framebufferResized = true;
	wrpWindow->width = width;
	wrpWindow->height = height;
}

ENGINE_END

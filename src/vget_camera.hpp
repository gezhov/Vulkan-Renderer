#pragma once

// libs
#define GLM_FORCE_RADIANS			  // Функции GLM будут работать с радианами, а не градусами
#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // GLM будет ожидать интервал нашего буфера глубины от 0 до 1 (например, для OpenGL используется интервал от -1 до 1)
#include <glm/glm.hpp>

namespace vget
{
	class VgetCamera
	{
	public:
		// Установка ортогонального объёма просмотра при помощи шести его плоскостей.
		void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far);

		// Задание формы ортогонального объёма для дальнейшей проекции перспективы.
		// fovy - vertical field of view, aspect - соотношение сторон, near и far - ближняя и дальняя плоскости отсечения.
		void setPerspectiveProjection(float fovy, float aspect, float near, float far);

		// Установка положения камеры. 'up' обозначает какое направление означает "вверх"
		void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});

		// Этот метод предназначен для закрепления направления камеры на конкретном объекте
		void setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});

		// Метод для установки ориентации камеры с помощью углов Эйлера (YXZ последовательность Тейта-Брайана)
		void setViewYXZ(glm::vec3 position, glm::vec3 rotation);
		
		const glm::mat4& getProjection() const {return projectionMatrix;}
		const glm::mat4& getView() const {return viewMatrix;}
		const glm::mat4& getInverseView() const {return inverseViewMatrix;}
		const glm::vec3 getPosition() const {return glm::vec3(inverseViewMatrix[3]);}

	private:
		glm::mat4 projectionMatrix{1.f}; // матрица проекции перспективы
		glm::mat4 viewMatrix{1.f}; // матрица просмотра (камеры)
		glm::mat4 inverseViewMatrix{1.f}; // матрица для обратного преобразования позиций из camera space в world space
	};
}
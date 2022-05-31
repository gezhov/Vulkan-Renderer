#pragma once

#include "vget_model.hpp"

// libs
#include <glm/gtc/matrix_transform.hpp>

// std
#include <memory>
#include <unordered_map>
#include <string>

namespace vget
{
	struct TransformComponent
	{
		glm::vec3 translation{}; // вектор для задания отступа в позиции
		glm::vec3 scale{ .1f, .1f, .1f }; // вектор со значениями для масштабирования
		glm::vec3 rotation{};

		// Построение матрицы аффинного преобразования следующим произведением = translate * Ry * Rx * Rz * scale
		// У произведения матриц нет коммутативного свойства, поэтому порядок множителей важен.
		// Представить преобразование можно "прочитав" произведение справа налево (сначала выполнится изменение размеров,
		// затем поворот поочерёдно по осям Z, X и Y, и в конце применится сдвиг).
		glm::mat4 mat4();

		// Построение матрицы нормали для приведения позиции нормалей вершин к мировому пространству (world space).
		// Эта матрица очень похожа на матрицу преобразования для самих вершин, за исключением некоторых моментов.
		glm::mat3 normalMatrix();
	};

	struct PointLightComponent
	{
		float lightIntensity = 1.0f;
		bool carouselEnabled = false;
	};

	class VgetGameObject
	{
	public:
		using id_t = unsigned int; // псевдоним для типа
		using Map = std::unordered_map<id_t, VgetGameObject>;

		VgetGameObject() = default; // Просит компилятор, хотя такой конструктор не используется

		// статичный метод, который выпускает новый экземпляр игрового объекта
		static VgetGameObject createGameObject(std::string name = "Object")
		{
			static id_t currentId = 0;
			return VgetGameObject{ currentId++, name };
		}

		// Метод для создания PointLight объекта
		static VgetGameObject makePointLight(float intensity = 10.f, float radius = 0.1f, glm::vec3 color = glm::vec3(1.f));

		// RAII
		VgetGameObject(const VgetGameObject&) = delete;
		VgetGameObject& operator=(const VgetGameObject&) = delete;
		
		// move constructor is default (используется для работы со ссылкой на объект через std::move)
		// например, чтобы добавить экземпляр объекта в коллекцию
		VgetGameObject(VgetGameObject&&) = default;
		VgetGameObject& operator=(VgetGameObject&&) = default;

		const id_t getId() { return id; }
		const std::string getName() { return name; }

		glm::vec3 color{};
		TransformComponent transform{};

		// Опциональные компоненты объекта в виде указателей.
		// Эти компоненты сигнализируют то, может ли данный объект исп. в конкретной системе движка.
		std::shared_ptr<VgetModel> model{};
		std::unique_ptr<PointLightComponent> pointLight = nullptr;

	private:
		VgetGameObject(id_t objId, std::string name) : id{objId}, name{name} { this->name.append(std::to_string(id)); }

		id_t id;
		std::string name;
	};
}

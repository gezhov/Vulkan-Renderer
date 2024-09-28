#pragma once

#include "Model.hpp"

// libs
#include <glm/gtc/matrix_transform.hpp>

// std
#include <memory>
#include <unordered_map>
#include <string>

struct TransformComponent
{
    glm::vec3 translation{};                  // отступ в позиции 
    glm::vec3 scale{ .1f, .1f, .1f };         // масштабирование модели 
    glm::vec3 rotation{3.15f, .0f, .0f};      // 3.15f в дефолт, потому что большинство моделей рендерятся перевёрнутыми при нуле (не флипнуты под Vulkan)

    // Построение матрицы аффинного преобразования следующим произведением = translate * Ry * Rx * Rz * scale
    // У произведения матриц нет коммутативного свойства, поэтому порядок множителей важен.
    // Представить преобразование можно "прочитав" произведение справа налево (сначала выполнится изменение размеров,
    // затем поворот поочерёдно по осям Z, X и Y, и в конце применится сдвиг).
    glm::mat4 modelMatrix();

    // Построение матрицы нормали для приведения позиции нормалей вершин к мировому пространству (world space).
    // Эта матрица очень похожа на матрицу преобразования для самих вершин, за исключением некоторых моментов.
    glm::mat3 normalMatrix();

    void fromModelMatrix(glm::mat4& modelMatrix);
};

struct PointLightComponent
{
    float lightIntensity = 1.0f;
    bool carouselEnabled = false;
};

class SceneObject
{
public:
    using id_t = unsigned int; // псевдоним для типа
    using Map = std::unordered_map<id_t, SceneObject>;

    SceneObject() = default; // Просит компилятор, хотя такой конструктор не используется

    // статичный метод, который выпускает новый экземпляр игрового объекта
    static SceneObject createSceneObject(std::string name = "Object")
    {
        static id_t currentId = 0;
        return SceneObject{ currentId++, name };
    }

    // Метод для создания PointLight объекта
    static SceneObject makePointLight(float intensity = 10.f, float radius = 0.1f, glm::vec3 color = glm::vec3(1.f));

    // RAII
    SceneObject(const SceneObject&) = delete;
    SceneObject& operator=(const SceneObject&) = delete;
    
    // move constructor is default (используется для работы со ссылкой на объект через std::move)
    // например, чтобы добавить экземпляр объекта в коллекцию
    SceneObject(SceneObject&&) = default;
    SceneObject& operator=(SceneObject&&) = default;

    const id_t getId() { return id; }
    const std::string getName() { return name; }

    glm::vec3 color{}; // being used for point light color
    TransformComponent transform{};

    // Опциональные компоненты объекта в виде указателей.
    // Эти компоненты сигнализируют то, может ли данный объект исп. в конкретной системе рендерера.
    std::shared_ptr<WrpModel> model{};
    std::unique_ptr<PointLightComponent> pointLight = nullptr;

private:
    SceneObject(id_t objId, std::string name) : id{objId}, name{name} { this->name.append(std::to_string(this->id)); }

    id_t id;
    std::string name;
};

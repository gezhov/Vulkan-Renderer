#include "SceneObject.hpp"

glm::mat4 TransformComponent::modelMatrix()
{
    // Ниже представлено оптимизированное создание матрицы афинных преобразований.
    // Она конструируется по столбцам. Первые три столбца это линейные преобразования,
    // а четвёртый столбец - вектор для сдвига объекта (translation).
    // Выражения для поворота по углам Эйлера (YXZ последовательность Тейта-Брайана)
    // взяты из википедии. Эти выражения получены после перемножения матриц всех трёх элементарных вращений.
    const float c3 = glm::cos(rotation.z);
    const float s3 = glm::sin(rotation.z);
    const float c2 = glm::cos(rotation.x);
    const float s2 = glm::sin(rotation.x);
    const float c1 = glm::cos(rotation.y);
    const float s1 = glm::sin(rotation.y);
    return glm::mat4{
        {
            scale.x * (c1 * c3 + s1 * s2 * s3),
            scale.x * (c2 * s3),
            scale.x * (c1 * s2 * s3 - c3 * s1),
            0.0f,
        },
        {
            scale.y * (c3 * s1 * s2 - c1 * s3),
            scale.y * (c2 * c3),
            scale.y * (c1 * c3 * s2 + s1 * s3),
            0.0f,
        },
        {
            scale.z * (c2 * s1),
            scale.z * (-s2),
            scale.z * (c1 * c2),
            0.0f,
        },
        {translation.x, translation.y, translation.z, 1.0f}};
}

glm::mat3 TransformComponent::normalMatrix()
{
    const float c3 = glm::cos(rotation.z);
    const float s3 = glm::sin(rotation.z);
    const float c2 = glm::cos(rotation.x);
    const float s2 = glm::sin(rotation.x);
    const float c1 = glm::cos(rotation.y);
    const float s1 = glm::sin(rotation.y);

    // Матрица масштабирования должна быть обратная
    const glm::vec3 invScale = 1.0f / scale;

    // В отличие от матрицы преобр. для вершин, здесь нет операции сдвига, так как он
    // не влияет на нормали. Следовательно, матрица сократилась до 3x3.
    return glm::mat3{
        {
            invScale.x * (c1 * c3 + s1 * s2 * s3),
            invScale.x * (c2 * s3),
            invScale.x * (c1 * s2 * s3 - c3 * s1),
        },
        {
            invScale.y * (c3 * s1 * s2 - c1 * s3),
            invScale.y * (c2 * c3),
            invScale.y * (c1 * c3 * s2 + s1 * s3),
        },
        {
            invScale.z * (c2 * s1),
            invScale.z * (-s2),
            invScale.z * (c1 * c2),
        }
    };
}

void TransformComponent::fromModelMatrix(glm::mat4& modelMatrix)
{
    // Extract translation directly from the model matrix
    translation = glm::vec3(modelMatrix[3]);

    // Extract scale factors by taking the length of each column vector
    scale.x = glm::length(glm::vec3(modelMatrix[0]));
    scale.y = glm::length(glm::vec3(modelMatrix[1]));
    scale.z = glm::length(glm::vec3(modelMatrix[2]));

    // Extract rotation component by converting the rotation matrix to Euler angles
    glm::mat3 rotationMatrix = glm::mat3(modelMatrix);

    // Normalize the rotation matrix
    rotationMatrix[0] /= scale.x;
    rotationMatrix[1] /= scale.y;
    rotationMatrix[2] /= scale.z;

    // Extract individual rotation matrix elements for convenience
    const float m00 = rotationMatrix[0][0];
    const float m01 = rotationMatrix[0][1];
    const float m02 = rotationMatrix[0][2];
    const float m10 = rotationMatrix[1][0];
    const float m11 = rotationMatrix[1][1];
    const float m12 = rotationMatrix[1][2];
    const float m20 = rotationMatrix[2][0];
    const float m21 = rotationMatrix[2][1];
    const float m22 = rotationMatrix[2][2];

    // Calculate yaw (rotation around Y-axis)
    rotation.y = atan2f(m20, m22);

    // Calculate pitch (rotation around X-axis)
    rotation.x = atan2f(-m21, sqrtf(m20 * m20 + m22 * m22));

    // Calculate roll (rotation around Z-axis)
    rotation.z = atan2f(m01, m11);
}

SceneObject SceneObject::makePointLight(float intensity, float radius, glm::vec3 color)
{
    SceneObject sceneObj = SceneObject::createSceneObject("PointLight");
    sceneObj.color = color;
    sceneObj.transform.scale.x = radius;  // радиус видимого билборда сохраняется в X-компоненту scale'а
    sceneObj.pointLight = std::make_unique<PointLightComponent>();
    sceneObj.pointLight->lightIntensity = intensity;
    return sceneObj;
}

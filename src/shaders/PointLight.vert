#version 450

// Значения для вычисления отступа позиции Point Light'а в X и Y направлениях пространства камеры.
// Полученные после сдвига позиции являются позициями вершин объекта билборда.
const vec2 OFFSETS[6] = vec2[](
  vec2(-1.0, -1.0), // треугольник раз
  vec2(-1.0, 1.0),
  vec2(1.0, -1.0),
  vec2(1.0, -1.0), // треугольник два
  vec2(-1.0, 1.0),
  vec2(1.0, 1.0)
);

// Выходная переменная отступа, которая будет линейно интерполирована во frag шейдере
layout (location = 0) out vec2 fragOffset;
 
struct PointLight {
    vec4 position; // w - игнорируется
    vec4 color;    // w - интенсивность цвета
};

// Ubo объект такой же как и в simple shader
layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 projection;
    mat4 view;
    mat4 invView;
    vec4 ambientLightColor;
    PointLight pointLights[10];
    int numLights;
    float diffuseProportion;
    float roughness;
    float indexOfRefraction;
} ubo;

// Пришедшая структура пуш-констант
layout(push_constant) uniform Push {
    vec4 position;
    vec4 color;
    float radius;
} push;

void main() {
    fragOffset = OFFSETS[gl_VertexIndex]; // gl_VertexIndex хранит индекс текущей обрабатываемой вершины

    // Извелечение векторов "вверх" и "вправо" из View матрицы (в данный момент это World Space)
    vec3 cameraRightWorld = {ubo.view[0][0], ubo.view[1][0], ubo.view[2][0]};
    vec3 cameraUpWorld = {ubo.view[0][1], ubo.view[1][1], ubo.view[2][1]};

    // Вычисление позиции вершины билборда в мировом пространстве
    vec3 positionWorld = push.position.xyz + push.radius * fragOffset.x * cameraRightWorld
        + push.radius * fragOffset.y * cameraUpWorld;

    // Перевод положения полученной вершины Point Light билборда в каноническое пространство
    gl_Position = ubo.projection * ubo.view * vec4(positionWorld, 1.0);

    /* Альтернативный вариант вычисления позиции вершины.
       Сначала позиция Point Light'а преобразуется в пространство камеры, затем
       в этом пространстве на неё применяется отступ и мы получаем позицию вершины
       в пространстве камеры. В конце полученная вершина преобразуется в каноническое пространство. */
//	vec4 lightInCameraSpace = ubo.view * vec4(ubo.lightPosition, 1.0);
//	vec4 positionInCameraSpace = lightInCameraSpace + LIGHT_RADIUS * vec4(fragOffset, 0.0, 0.0);
//	gl_Position = ubo.projection * positionInCameraSpace;
}

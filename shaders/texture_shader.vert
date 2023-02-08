#version 450

/*
vec2 positions[3] = vec2[] (
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
); */

// Захардкоженные позиции вершин заменяются переменной postion,
// значение которой берётся из соответствующего атрибута буфера вершин.
// Квалификатор in определяет эту переменную как входную.
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;			// атрибут цвета для данной вершины
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;			// координата текстуры

// Выходные переменные участвуют в дальнейшем интерполировании и передаче в шейдер фрагмента.
// Они могут повторять индекс местоположения (location) входных переменных, т.к. in и out переменные не имеют связи.
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosWorld;
layout(location = 2) out vec3 fragNormalWorld;
layout(location = 3) out vec2 fragUv;

struct PointLight {
    vec4 position; // w - игнорируется
    vec4 color;    // w - интенсивность цвета
};

// Тип, который получает данные из унифицированного буфера с ubo объектом внутри.
// Такой read only buffer передаётся через набор дескрипторов, в котором он содержится
// по указанной привязке.
layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 projection;
    mat4 view;
    mat4 invView;
    vec4 ambientLightColor;
    PointLight pointLights[10];
    int numLights;
} ubo;

// Блок, который получает значения из структуры пуш-констант. Блок пуш-констант должен быть
// только один для одного шейдера, а его порядок полей должен совпадать со структурой, записанной в буфере команд.
layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
    int textureIndex;
    vec3 diffuseColor;
} push;

void main() {
    // Если вектор обозначает направление, то однородную координату нужно заменить на 0,
    // чтобы на вектор не применился сдвиг (translation).
    vec4 positionWorld = push.modelMatrix * vec4(position, 1.0); // перевод позиции вершины в мировое пространство

    // Дополнительное применение аффинного преобразования (projectionViewMatrix * positionWorld).
    gl_Position = ubo.projection * ubo.view * positionWorld;

    // Вектор нормали для вершины приводится в координаты мирового пространства.
    // Решение ниже работает только для случая, если scale модели равномерный, т.е. (s*x == s*y == s*z)
    // vec3 normalWorldSpace = normalize(normalMatrix * normal);

    // Для осуществления корректных преобразований нормали, матрица модели сначала инвертируется,
    // а затем транспонируется.
    //mat3 normalMatrix = transpose(inverse(mat3(push.modelMatrix)));
    //vec3 normalWorldSpace = normalize(normalMatrix * normal);
    // Нахождение матрицы нормали было вынесено на сторону хоста.

    fragNormalWorld = normalize(mat3(push.normalMatrix) * normal);
    fragPosWorld = positionWorld.xyz;
    fragColor = color;
    fragUv = uv;

    // прежние строки
    //gl_Position = vec4(push.transform * position + push.offset, 0.0, 1.0);
}

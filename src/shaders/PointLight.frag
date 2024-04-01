#version 450

layout (location = 0) in vec2 fragOffset;
layout (location = 0) out vec4 outColor;

struct PointLight {
    vec4 position; // w - игнорируется
    vec4 color;    // w - интенсивность цвета
};

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

layout(push_constant) uniform Push {
    vec4 position;
    vec4 color;
    float radius;
} push;

const float M_PI = 3.1415926536;

void main() {
    float dis = sqrt(dot(fragOffset, fragOffset)); // вычисляем дистанцию от позиции Point Light'а до фрагмента

    // Если дистанция > 1, значит фрагмент вышел за заданный радиус и мы его отбрасываем, чтобы сделать круглый билборд
    if (dis >= 1.0) {
        discard; // отбросить данный фрагмент и вернуться из функции
    }

    // Alpha-компонент изменяется в зависимости от расстояния до фрагмента, создавая эффект прозрачности для билборда PointLight'а.
    // Это позволяет делать выражение с функцией косинуса от дистанции (cosDis). Также, прибавив cosDis к цвету фрагмента,
    // был получен переход цвета от белого в центре билборда к реальному цвету поинт лайта ближе к его краям.
    float cosDis = 0.5 * (cos(dis * M_PI) + 1.0);
    outColor = vec4(push.color.xyz + cosDis, cosDis);
}

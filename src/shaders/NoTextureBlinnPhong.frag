#version 450

// Входные интерполированные от трёх вершин переменные. Location и тип данных должны совпадать с выходными переменными из шейдера вершин.
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;
layout(location = 3) in vec2 fragUv;

layout(location = 0) out vec4 outColor;

struct PointLight {
    vec4 position; // w - игнорируется
    vec4 color;    // w - интенсивность цвета
};

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
    vec3 diffuseColor;
} push;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 projection;
    mat4 view;
    mat4 invView;
    vec4 ambientLightColor;
    float directionalLightIntensity;
    vec4 directionalLightPosition;
    PointLight pointLights[10];
    int numLights;
    float diffuseProportion;
    float roughness;
    float indexOfRefraction;
} globalUbo;

// Source of directional light
vec3 DIRECTION_TO_LIGHT = normalize(globalUbo.directionalLightPosition.xyz);

void main() {
    vec3 diffuseLight = globalUbo.ambientLightColor.xyz * globalUbo.ambientLightColor.w;
    vec3 specularLight = vec3(0.0);
    float specularProportion = 1.0 - globalUbo.diffuseProportion; // energy conservation rule
    // Нормаль поверхности одинакова для всех источников света, поэтому она нормализуется вне цикла.
    // Нормализовывать её надо, потому что она пришла в шейдер фрагментов после интерполяции из нескольких нормалей вершин.
    vec3 surfaceNormal = normalize(fragNormalWorld);

    vec3 cameraPosWorld = globalUbo.invView[3].xyz; // извлекаем позицию наблюдателя в World Space из обратной матрицы просмотра
    vec3 viewDirection = normalize(cameraPosWorld - fragPosWorld); // направление до наблюдателя

    // Вклад направленного источника света в рассеянное освещение
    diffuseLight += max(dot(surfaceNormal, DIRECTION_TO_LIGHT), 0) * globalUbo.directionalLightIntensity;

    for (int i = 0; i < globalUbo.numLights; ++i) {
        PointLight light = globalUbo.pointLights[i];

        // --- diffuse term ---
        vec3 directionToLight = light.position.xyz - fragPosWorld; // ещё ненормализованное направление к ист. света
        float attenuation = 1.0 / dot(directionToLight, directionToLight); // фактор ослабевания интенсивности света = 1 / квадрат расстояния до источника
        directionToLight = normalize(directionToLight);
        float NdotL = max(dot(surfaceNormal, directionToLight), 0); // cosine of the angle of incidence
        vec3 intensity = light.color.xyz * light.color.w * attenuation;

        diffuseLight += globalUbo.diffuseProportion * intensity * NdotL;

        // --- specular term ---
        vec3 halfAngleVec = normalize(directionToLight + viewDirection); // H
        float NdotH = max(dot(surfaceNormal, halfAngleVec), 0.0);    // фактор-член влияния зеркального света по Блинн-Фонгу на интенсивность отражённого света
        NdotH = pow(NdotH, 60.0);  // больше степень => резче блик отражённого света

        specularLight += specularProportion * intensity * NdotH;
    }

    // Если "простая модель" пришла на вход без цвета, то используется серый оттенок,
    // чтобы чёрный цвет не сводил к нулю все операции.
    if (fragColor == vec3(0.0)) {
        vec3 greyShadeColor = vec3(0.02);
        outColor = vec4(diffuseLight * greyShadeColor + specularLight * greyShadeColor, 1.0);
    }
    else {
        outColor = vec4(diffuseLight * fragColor + specularLight * fragColor, 1.0);
    }
}

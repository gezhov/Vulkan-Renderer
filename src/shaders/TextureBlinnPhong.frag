#version 450

#define TEXTURES_COUNT 0

// Input variables interpolated from 3 vertcies
layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec3 fragPosWorld;
layout (location = 2) in vec3 fragNormalWorld;
layout (location = 3) in vec2 fragUv;

layout (location = 0) out vec4 outColor;

struct PointLight {
    vec4 position; // w - ignored
    vec4 color;    // w - color intensity
};

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
    int diffTexIndex;
    int specTexIndex;
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
    vec3 surfaceNormal = normalize(fragNormalWorld);

    vec3 cameraPosWorld = globalUbo.invView[3].xyz;
    vec3 viewDirection = normalize(cameraPosWorld - fragPosWorld);

    diffuseLight += max(dot(surfaceNormal, DIRECTION_TO_LIGHT), 0) * globalUbo.directionalLightIntensity;

    for (int i = 0; i < globalUbo.numLights; ++i) {
        PointLight light = globalUbo.pointLights[i];

        // --- diffuse term ---
        vec3 directionToLight = light.position.xyz - fragPosWorld;
        float attenuation = 1.0 / dot(directionToLight, directionToLight);
        directionToLight = normalize(directionToLight);
        float cosAngIncidence = max(dot(surfaceNormal, directionToLight), 0);
        vec3 intensity = light.color.xyz * light.color.w * attenuation;

        diffuseLight += globalUbo.diffuseProportion * intensity * cosAngIncidence;

        // --- specular term ---
        vec3 halfAngleVec = normalize(directionToLight + viewDirection);
        float blinnTerm = dot(surfaceNormal, halfAngleVec); // фактор-член влияния зеркального света по Блинн-Фонгу на интенсивность отражённого света
        blinnTerm = clamp(blinnTerm, 0, 1);  // отбросить случаи, когда источник света или наблюдатель находятся с другой стороны поверхности
        blinnTerm = pow(blinnTerm, 60.0);  // больше степень => резче блик отражённого света

        specularLight += specularProportion * intensity * blinnTerm;
    }

    // Фрагмент получает цвет по координатам текстуры,
    // либо диффузный цвет своего материала, если для него текструра отсутствует.
    vec4 sampleTextureColor = vec4(0.8, 0.1, 0.1, 1);
    vec4 specularColor = vec4(0.0, 0.0, 0.0, 1);
    if (push.diffTexIndex != -1) {
#ifdef TEXTURES
        sampleTextureColor = texture(texSampler[push.diffTexIndex], fragUv);
#endif
    } else {
        sampleTextureColor = vec4(push.diffuseColor, 1.0);
    }

    if (push.specTexIndex != -1) {
#ifdef TEXTURES
        specularColor = texture(texSampler[push.specTexIndex], fragUv);
#endif
    } else {
        specularColor = sampleTextureColor;
    }

    //outColor = sampleTextureColor; // просто текстура
    outColor = vec4(diffuseLight * sampleTextureColor.rgb + specularLight * specularColor.rgb, 1.0);
}

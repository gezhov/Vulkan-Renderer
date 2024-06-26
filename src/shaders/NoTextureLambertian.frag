#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;
layout(location = 3) in vec2 fragUv;

layout(location = 0) out vec4 outColor;

struct PointLight {
    vec4 position; // w - ignored
    vec4 color;    // w - color intensity
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
    vec3 surfaceNormal = normalize(fragNormalWorld);

    // Directional light contribution
    diffuseLight += max(dot(surfaceNormal, DIRECTION_TO_LIGHT), 0) * globalUbo.directionalLightIntensity;

    for (int i = 0; i < globalUbo.numLights; ++i) {
        PointLight light = globalUbo.pointLights[i];

        vec3 directionToLight = light.position.xyz - fragPosWorld;
        float attenuation = 1.0 / dot(directionToLight, directionToLight); // intensity attenuation factor
        directionToLight = normalize(directionToLight);

        float NdotL = max(dot(surfaceNormal, directionToLight), 0); // cosine of the angle of incidence 
        vec3 intensity = light.color.xyz * light.color.w * attenuation;

        diffuseLight += intensity * NdotL;
    }

    // using dark grey for black (no color) models to see light impact
    if (fragColor == vec3(0.0)) {
        vec3 greyShadeColor = vec3(0.02);
        outColor = vec4(diffuseLight * greyShadeColor, 1.0);
    }
    else {
        outColor = vec4(diffuseLight * fragColor, 1.0);
    }
}
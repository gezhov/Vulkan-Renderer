#version 450

#define TEXTURES_COUNT 0

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
    int textureIndex;
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
} globalUbo;

// Source of directional light
vec3 DIRECTION_TO_LIGHT = normalize(globalUbo.directionalLightPosition.xyz);

void main() {
    vec3 diffuseLight = globalUbo.ambientLightColor.xyz * globalUbo.ambientLightColor.w;
    vec3 surfaceNormal = normalize(fragNormalWorld);

    vec3 cameraPosWorld = globalUbo.invView[3].xyz; // getting cameras world space pos from inverse view matrix
    vec3 viewDirection = normalize(cameraPosWorld - fragPosWorld); // Eye vector

    // Directional light contribution
    diffuseLight += max(dot(surfaceNormal, DIRECTION_TO_LIGHT), 0) * globalUbo.directionalLightIntensity;

    for (int i = 0; i < globalUbo.numLights; ++i) {
        PointLight light = globalUbo.pointLights[i];

        vec3 directionToLight = light.position.xyz - fragPosWorld;
        float attenuation = 1.0 / dot(directionToLight, directionToLight); // intensity attenuation factor
        directionToLight = normalize(directionToLight);

        float cosAngIncidence = max(dot(surfaceNormal, directionToLight), 0);
        vec3 intensity = light.color.xyz * light.color.w * attenuation;

        diffuseLight += intensity * cosAngIncidence;
    }

    // Fragment getting texture color by coordinates if it's present
    // and materials diffuse color otherwise.
    vec4 sampleTextureColor = vec4(0.8, 0.1, 0.1, 1);
    if (push.textureIndex != -1) {
#ifdef TEXTURES
        sampleTextureColor = texture(texSampler[push.textureIndex], fragUv);
#endif
    } else {
        sampleTextureColor = vec4(push.diffuseColor, 1.0);
    }

    //outColor = sampleTextureColor;
    outColor = vec4(diffuseLight * sampleTextureColor.rgb, 1.0);
}

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
    vec3 specularLight = vec3(0.0);
    vec3 surfaceNormal = normalize(fragNormalWorld);

    vec3 cameraPosWorld = globalUbo.invView[3].xyz; // getting cameras world space pos from inverse view matrix
    vec3 viewDirection = normalize(cameraPosWorld - fragPosWorld); // Eye vector

    // Directional light contribution
    diffuseLight += max(dot(surfaceNormal, DIRECTION_TO_LIGHT), 0) * globalUbo.directionalLightIntensity;

    for (int i = 0; i < globalUbo.numLights; ++i) {
        PointLight light = globalUbo.pointLights[i];

        // diffuse term
        vec3 directionToLight = light.position.xyz - fragPosWorld;
        float attenuation = 1.0 / dot(directionToLight, directionToLight); // intensity attenuation factor
        directionToLight = normalize(directionToLight);
        float cosAngIncidence = max(dot(surfaceNormal, directionToLight), 0);
        vec3 intensity = light.color.xyz * light.color.w * attenuation;
        diffuseLight += intensity * cosAngIncidence;
        
        // specular term (Torrance-Sparrow microfacet model, source: [Blinn, 1977])
        // s = D * G * F / (N * E)
        // Magnesium Oxide Ceramic values for c3 and n is used for now.
        // D - Facet distribution function (D3 - Trowbridge-Reitz function)
        float c3 = 0.35; // [0;1] <=> [specular;diffuse]
        // H - halfway direction
        vec3 H = normalize((directionToLight + viewDirection) / length(directionToLight + viewDirection));
        float c3sqrd = pow(c3,2);
        float D = pow(c3sqrd / ((dot(surfaceNormal,H) * (c3sqrd-1))+1), 2);
        // (N * E) - consine of inclination angle
        float NdotE = dot(surfaceNormal, viewDirection);
        // G - geometrical attenuation factor
        float G = 0;
        float NdotL = dot(surfaceNormal, directionToLight);
        float NdotH = dot(surfaceNormal, H);
        float HdotE = dot(H, viewDirection);
        if (NdotE < NdotL) {
            if (2 * NdotE * NdotH < HdotE) G = 2 * NdotH / HdotE;
            else G = 1 / NdotE;
        }
        else {
            if (2 * NdotL * NdotH < HdotE) G = 2 * NdotH * NdotL / HdotE * NdotE;
            else G = 1 / NdotE;
        }
        // F - Frenel reflection
        float n = 1.8; // index of refraction
        float c = HdotE;
        float g = sqrt(pow(n,2) + pow(c,2) - 1);
        float F = pow(g-c,2)/pow(g+c,2) * (1 + pow(c * (g+c) - 1, 2)/pow(c * (g-c) + 1, 2));
        specularLight += intensity * D * G * F / NdotE;
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
    outColor = vec4(diffuseLight * sampleTextureColor.rgb + specularLight * sampleTextureColor.rgb, 1.0);
}

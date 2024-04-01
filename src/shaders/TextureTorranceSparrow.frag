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
    float diffuseProportion;
    float roughness;
    float indexOfRefraction;
} globalUbo;

// --- Functions ---
// Geometrical attenuation - Schlick-GGX
float G1(float alpha, float NdotX);             // Schlick-Beckmann geometry shadowing function
float G(float alpha, float NdotE, float NdotL); // Smith model
// Schlick's Fresnel factor approximation
float FresnelSchlick(float n, float HdotE);

// Geometrical attenuation from [Jim Blinn, 1977] paper
float geometricalAttenuation_Blinn1977(float NdotL, float NdotH, float NdotE, float HdotE);
// Fresnel function from [Jim Blinn, 1977] paper
float Fresnel_Blinn1977(float n, float HdotE);
// ---    ---

// Source of directional light
vec3 DIRECTION_TO_LIGHT = normalize(globalUbo.directionalLightPosition.xyz);

void main() {
    vec3 diffuseLight = globalUbo.ambientLightColor.xyz * globalUbo.ambientLightColor.w;
    vec3 specularLight = vec3(0.0);
    float specularProportion = 1.0 - globalUbo.diffuseProportion; // energy conservation rule
    vec3 surfaceNormal = normalize(fragNormalWorld);

    vec3 cameraPosWorld = globalUbo.invView[3].xyz; // getting cameras world space pos from inverse view matrix
    vec3 viewDirection = normalize(cameraPosWorld - fragPosWorld); // Eye vector

    // Directional light contribution
    diffuseLight += max(dot(surfaceNormal, DIRECTION_TO_LIGHT), 0) * globalUbo.diffuseProportion * globalUbo.directionalLightIntensity;

    for (int i = 0; i < globalUbo.numLights; ++i) {
        PointLight light = globalUbo.pointLights[i];

        // --- diffuse term ---
        vec3 directionToLight = light.position.xyz - fragPosWorld;
        float attenuation = 1.0 / dot(directionToLight, directionToLight); // intensity attenuation factor
        directionToLight = normalize(directionToLight);
        float NdotL = max(dot(surfaceNormal, directionToLight), 0.0); // aka cosine of the angle of incidence
        vec3 intensity = light.color.xyz * light.color.w * attenuation;
        diffuseLight += globalUbo.diffuseProportion * intensity * NdotL;
        
        // --- specular term --- (Torrance-Sparrow microfacet model, source: [Blinn, 1977])
        // s = D * G * F / (N * E)
        // Magnesium Oxide Ceramic values for c3 and n is used for now.

        // D - Facet distribution function (D3 - Trowbridge-Reitz (GGX) function)
        float c3 = globalUbo.roughness; // [0;1] <=> [specular;diffuse]
        vec3 H = normalize((directionToLight + viewDirection) / length(directionToLight + viewDirection)); // Halfway direction
        float c3sqrd = pow(c3,2);
        float NdotH = max(dot(surfaceNormal, H), 0.0);
        float D = pow(c3sqrd / (pow(NdotH,2) * (c3sqrd-1)+1), 2);

        // (N * E) - cosine of inclination angle
        float NdotE = max(dot(surfaceNormal, viewDirection), 0.0);

        // G - geometrical attenuation factor.
        float HdotE = max(dot(H, viewDirection), 0.0);
        //float G = geometricalAttenuation_Blinn1977(NdotL, NdotH, NdotE, HdotE);
        float alpha = pow(c3, 2);
        float G = G(alpha, NdotE, NdotL);

        // F - Frenel reflection
        //float F = Fresnel_Blinn1977(globalUbo.indexOfRefraction, HdotE);
        float F = FresnelSchlick(globalUbo.indexOfRefraction, HdotE);

        //specularLight += specularProportion * intesity * D * G * F; // Torrance-Sparrow equation for [Blinn, 1977] G variant.
        specularLight += specularProportion * intensity * D * G * F / max(NdotE, 0.000001);
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

float G1(float alpha, float NdotX)
{
    float numerator = NdotX;
    float k = alpha / 2.0;
    float denominator = NdotX * (1.0 - k) + k;
    denominator = max(denominator, 0.000001);
    return numerator / denominator;
}

float G(float alpha, float NdotE, float NdotL)
{
    return G1(alpha, NdotE) * G1(alpha, NdotL);
}

float FresnelSchlick(float n, float HdotE)
{
    float F0 = pow(n-1, 2)/pow(n+1, 2);
    return F0 + (1 - F0) * pow(1 - HdotE, 5);
}

float geometricalAttenuation_Blinn1977(float NdotL, float NdotH, float NdotE, float HdotE)
{
    // 1/NdotE factor from main equation is combined here, therefore
    // it's not needed in the final equasion for specular light.
    float G = 0;
    if (NdotE < NdotL) {
        if (2 * NdotE * NdotH < HdotE) G = 2 * NdotH / HdotE;
        else G = 1 / NdotE;
    }
    else {
        if (2 * NdotL * NdotH < HdotE) G = 2 * NdotH * NdotL / HdotE * NdotE;
        else G = 1 / NdotE;
    }
    return G;
}

float Fresnel_Blinn1977(float n, float HdotE)
{
    float c = HdotE;
    float g = sqrt(pow(n,2) + pow(c,2) - 1);
    return pow(g-c,2)/pow(g+c,2) * (1 + pow(c * (g+c) - 1, 2)/pow(c * (g-c) + 1, 2));
}

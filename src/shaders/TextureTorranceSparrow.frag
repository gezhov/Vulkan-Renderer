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
// Torrance-Sparrow microfacet model, source: [Blinn, 1977]
float Specular_TorranceSparrow_Blinn1977(float alpha, float NdotL, float NdotH, float NdotE, float HdotE);
// ---    ---

// Source of directional light
vec3 DIRECTION_TO_LIGHT = normalize(globalUbo.directionalLightPosition.xyz);

void main() {
   vec3 diffuseLight = globalUbo.ambientLightColor.xyz * globalUbo.ambientLightColor.w;
    vec3 specularLight = vec3(0.0);
    //float specularProportion = 1.0 - globalUbo.diffuseProportion; // energy conservation rule (manual regulation)
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
        float NdotL = max(dot(surfaceNormal, directionToLight), 0.0); // cosine of the angle of incidence 
        vec3 intensity = light.color.xyz * light.color.w * attenuation;
        
        vec3 H = normalize((directionToLight + viewDirection) / length(directionToLight + viewDirection)); // Halfway direction
        float HdotE = max(dot(H, viewDirection), 0.0);
        float F = FresnelSchlick(globalUbo.indexOfRefraction, HdotE);
        float specularProportion = F;
        float diffuseProportion = 1.0 - specularProportion;

        float c3 = globalUbo.roughness; // [0;1] <=> [specular;diffuse]
        float alpha = pow(c3,2);
        float NdotH = max(dot(surfaceNormal, H), 0.0);
        float D = pow(alpha / (pow(NdotH,2) * (alpha-1)+1), 2);

        float NdotE = max(dot(surfaceNormal, viewDirection), 0.0); // (N * E) - cosine of inclination angle
        float G = G(alpha, NdotE, NdotL);

        diffuseLight += diffuseProportion * intensity * NdotL;
        // Cook-Torrance specular reflection model
        specularLight += specularProportion * intensity * D * G * F / 4 * max(NdotL, 0.000001) * max(NdotE, 0.000001);
        // Torrance-Sparrow [Blinn, 1977]
        //specularLight += specularProportion * intensity * Specular_TorranceSparrow_Blinn1977(alpha, NdotL, NdotH, NdotE, HdotE);
    }

    // Fragment getting texture color by coordinates if it's present
    // and materials diffuse color otherwise.
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

    //outColor = sampleTextureColor;
    outColor = vec4(diffuseLight * sampleTextureColor.rgb + specularLight * specularColor.rgb, 1.0);
}

float Specular_TorranceSparrow_Blinn1977(float alpha, float NdotL, float NdotH, float NdotE, float HdotE)
{
    // D - Facet distribution function (D3 - Trowbridge-Reitz (GGX) function)
    float D = pow(alpha / (pow(NdotH,2) * (alpha-1)+1), 2);
    // G - geometrical attenuation factor.
    float G = geometricalAttenuation_Blinn1977(NdotL, NdotH, NdotE, HdotE);
    // F - Frenel reflection
    float F = Fresnel_Blinn1977(globalUbo.indexOfRefraction, HdotE);
    // 1 / NdotE is used in G factor so its omitted
    return D * G * F; // Torrance-Sparrow equation for [Blinn, 1977] (1/NdotE in G variant).
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

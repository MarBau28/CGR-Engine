#version 330 core

// Input
in vec2 fragTexCoord;

// Out to screen
out vec4 finalColor;

// Uniforms
uniform vec2 resolution;
uniform vec3 viewPos;

// Global settings
uniform vec3 backgroundColor;
uniform float lightIntensity;
uniform float ambientLightStrength;
uniform float specularStrength;
uniform int shininess;
uniform int activeLights;
uniform float maxLightRadius;
uniform float attenuationConstant = 1.0;
uniform float attenuationLinear = 0.09;
uniform float attenuationQuadratic = 0.032;

// G-Buffer
uniform sampler2D texture0;
uniform sampler2D gNormalTex;
uniform sampler2D gPositionTex;

// MRT Light Arrays
#define MAX_LIGHTS 500
uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];

void main()
{
    //    vec3 effectiveLight = lightColor * lightIntensity;

    // Extract data from G-Buffer
    vec4 albedoData = texture(texture0, fragTexCoord);
    vec3 albedo = albedoData.rgb;
    vec3 rawNormal = texture(gNormalTex, fragTexCoord).rgb;
    vec3 fragPosition = texture(gPositionTex, fragTexCoord).rgb;

    // Test: standard textures
    //    finalColor = vec4(albedo, 1.0);
    // Test: Normal vectors mapped as colors
    //    finalColor = vec4(normalize(rawNormal) * 0.5 + 0.5, 1.0);
    // Test: raw World Position coordinates mapped as colors
    //    finalColor = vec4(fragPosition * 0.05, 1.0);

    // Masking check (for light sources and sky box)
    if (length(rawNormal) < 0.1) {
        // If empty space draw backgroundColor
        if (albedoData.a == 0.0) {
            finalColor = vec4(backgroundColor, 1.0);
        }
        // If Alpha is > 0.0, it's normal object that should not be shaded
        else {
            finalColor = vec4(albedo, 1.0);
        }
        return;
    }

    // normalize the rawNormal for further lightning calcs
    vec3 normal = normalize(rawNormal);
    vec3 viewDir = normalize(viewPos - fragPosition);

    // Global Ambient
    vec3 totalLighting = vec3(1.0) * ambientLightStrength;

    for (int i = 0; i < activeLights; i++) {

        vec3 currentEffectiveLight = lightColors[i] * lightIntensity;
        vec3 lightVector = lightPositions[i] - fragPosition;
        float lightDistance = length(lightVector);

        // If the light is to far away ligth calc can be skipped
        if (lightDistance > maxLightRadius) continue;

        vec3 lightDir = normalize(lightVector);

        // Diffuse
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = diff * currentEffectiveLight;

        // Specular
        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
        vec3 specular = specularStrength * spec * currentEffectiveLight;

        // Attenuation
        float attenuation = 1.0 /
        (attenuationConstant + attenuationLinear * lightDistance
        + attenuationQuadratic * (lightDistance * lightDistance));
        float distanceRatio = lightDistance / maxLightRadius;
        float distanceRatioQuad = distanceRatio * distanceRatio * distanceRatio
        * distanceRatio;
        float windowing = clamp(1.0 - distanceRatioQuad, 0.0, 1.0);
        attenuation *= windowing;

        // Add this light's contribution to the total
        totalLighting += (diffuse + specular) * attenuation;
    }

    // Combine Lightning
    vec3 color = totalLighting * albedo;

    // Output
    finalColor = vec4(color, albedoData.a);
}
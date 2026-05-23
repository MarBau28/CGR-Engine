#version 330 core

in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragPosition;

out vec4 finalColor;

uniform float lightIntensity;
uniform float ambientLightStrength;
uniform int activeLights;
uniform float maxLightRadius;
uniform float attenuationConstant = 1.0;
uniform float attenuationLinear = 0.09;
uniform float attenuationQuadratic = 0.032;

#define MAX_LIGHTS 500
uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColor;

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 totalToonLighting = vec3(0.0);
    vec3 toonBaseColor = vec3(1.0, 0.275, 0.333);
    float levels = 8.0;

    for (int i = 0; i < activeLights; i++) {
        vec3 lightVector = lightPositions[i] - fragPosition;
        float lightDistance = length(lightVector);
        if (lightDistance > maxLightRadius) continue;

        vec3 lightDir = normalize(lightVector);
        float diff = max(dot(normal, lightDir), 0.0);

        float level = floor(diff * levels);
        diff = level / levels;
        vec3 diffuse = diff * lightColor * lightIntensity;

        float attenuation = 1.0 / (attenuationConstant + attenuationLinear * lightDistance + attenuationQuadratic * (lightDistance * lightDistance));
        float distanceRatio = lightDistance / maxLightRadius;
        float distanceRatioQuad = distanceRatio * distanceRatio * distanceRatio * distanceRatio;
        attenuation *= clamp(1.0 - distanceRatioQuad, 0.0, 1.0);

        totalToonLighting += diffuse * attenuation;
    }

    vec3 ambient = ambientLightStrength * toonBaseColor;
    finalColor = vec4(ambient + (totalToonLighting * toonBaseColor), 1.0);
}
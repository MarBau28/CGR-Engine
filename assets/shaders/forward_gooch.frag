#version 330 core

in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragPosition;

out vec4 finalColor;

uniform vec3 viewPos;

uniform float lightIntensity;
uniform int activeLights;
uniform float maxLightRadius;
uniform float attenuationConstant = 1.0;
uniform float attenuationLinear = 0.09;
uniform float attenuationQuadratic = 0.032;

#define MAX_LIGHTS 500
uniform vec3 lightPositions[MAX_LIGHTS];

const int shininess = 48;

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(viewPos - fragPosition);
    vec3 totalLighting = vec3(0.0);

    vec3 surfaceColor = vec3(0.8, 0.4, 0.1);
    vec3 coolColor = vec3(0.0, 0.0, 0.6) + 0.2 * surfaceColor;
    vec3 warmColor = vec3(0.6, 0.6, 0.0) + 0.6 * surfaceColor;

    for (int i = 0; i < activeLights; i++) {
        vec3 lightVector = lightPositions[i] - fragPosition;
        float lightDistance = length(lightVector);
        if (lightDistance > maxLightRadius) continue;

        vec3 lightDir = normalize(lightVector);
        float t = (dot(normal, lightDir) + 1.0) / 2.0;
        vec3 goochDiffuse = mix(coolColor, warmColor, t);

        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess * 4.0);
        vec3 specular = vec3(1.0) * spec;

        float attenuation = 1.0 / (attenuationConstant + attenuationLinear * lightDistance + attenuationQuadratic * (lightDistance * lightDistance));
        float distanceRatio = lightDistance / maxLightRadius;
        float distanceRatioQuad = distanceRatio * distanceRatio * distanceRatio * distanceRatio;
        attenuation *= clamp(1.0 - distanceRatioQuad, 0.0, 1.0);

        totalLighting += (goochDiffuse + specular) * attenuation * lightIntensity;
    }

    finalColor = vec4(totalLighting, 1.0);
}
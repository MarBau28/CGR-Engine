#version 330 core

in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragPosition;

out vec4 finalColor;

uniform sampler2D texture0;
uniform vec3 viewPos;

// Shared Light Uniforms
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

const float specularStrength = 1.0;
const int shininess = 48;

void main()
{
    vec3 albedo = texture(texture0, fragTexCoord).rgb;
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(viewPos - fragPosition);
    vec3 totalLighting = vec3(1.0) * ambientLightStrength;

    for (int i = 0; i < activeLights; i++) {
        vec3 lightVector = lightPositions[i] - fragPosition;
        float lightDistance = length(lightVector);
        if (lightDistance > maxLightRadius) continue;

        vec3 lightDir = normalize(lightVector);
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = diff * lightColor * lightIntensity;

        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
        vec3 specular = specularStrength * spec * lightColor * lightIntensity;

        float attenuation = 1.0 / (attenuationConstant + attenuationLinear * lightDistance + attenuationQuadratic * (lightDistance * lightDistance));
        float distanceRatio = lightDistance / maxLightRadius;
        float distanceRatioQuad = distanceRatio * distanceRatio * distanceRatio * distanceRatio;
        attenuation *= clamp(1.0 - distanceRatioQuad, 0.0, 1.0);

        totalLighting += (diffuse + specular) * attenuation;
    }

    finalColor = vec4(totalLighting * albedo, 1.0);
}
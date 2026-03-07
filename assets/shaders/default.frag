#version 330 core

// in from vertex shader
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragPosition;

// out to screen
out vec4 finalColor;

// Uniforms
uniform sampler2D texture0; // Raylib's default texture sampler namess
uniform vec3 lightPos; // light source position
uniform vec3 viewPos; // camera position

void main()
{
    // Light settings
    vec3 lightColor = vec3(1.0, 0.9, 0.75);
    float lightIntensity = 2.0;
    float ambientLightStrength = 0.15f;
    float specularStrength = 0.5;
    int shininess = 48;
    float attenuationConstant = 1.0; // light distance (attenuation)
    float attenuationLinear = 0.09; // light distance (attenuation)
    float attenuationQuadratic = 0.032; // light distance (attenuation)
    vec3 effectiveLight = lightColor * lightIntensity;

    // calculate ambient light color
    vec3 ambient = ambientLightStrength * lightColor;

    // calculate diffuse lighting
    vec3 lightVector = lightPos - fragPosition;
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(lightVector);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * effectiveLight;

    // calculate specular lighting
    vec3 viewDir = normalize(viewPos - fragPosition);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * effectiveLight;

    // calculate attenuation (ligth distance amplification)
    float lightDistance = length(lightVector);
    float attenuation = 1.0 / (attenuationConstant + attenuationLinear * lightDistance
    + attenuationQuadratic * (lightDistance * lightDistance));
    diffuse *= attenuation;
    specular *= attenuation;

    // calculate textur color
    vec4 textureMapping = texture(texture0, fragTexCoord);

    // final output: ambient light color + diffuse light color * texture color
    vec3 color = (ambient + diffuse + specular) * textureMapping.rgb;
    finalColor = vec4(color, textureMapping.a);
}
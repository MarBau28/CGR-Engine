#version 330 core

// Input
in vec2 fragTexCoord;

// out to screen
out vec4 finalColor;

// Uniforms
uniform vec2 resolution;
uniform vec3 lightPos;
uniform vec3 viewPos; // camera

// static vars from settings
uniform vec3 backgroundColor;
uniform vec3 lightColor;
uniform float lightIntensity;
uniform float ambientLightStrength;
uniform float specularStrength;
uniform int shininess;

// G-Buffer
uniform sampler2D texture0;
uniform sampler2D gNormalTex;
uniform sampler2D gPositionTex;

// light distance (attenuation) constants
const float attenuationConstant = 1.0;
const float attenuationLinear = 0.09;
const float attenuationQuadratic = 0.032;

void main()
{
    vec3 effectiveLight = lightColor * lightIntensity;

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

    // Ambient
    vec3 ambient = ambientLightStrength * lightColor;

    // Diffuse
    vec3 lightVector = lightPos - fragPosition;
    vec3 lightDir = normalize(lightVector);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * effectiveLight;

    // Specular
    vec3 viewDir = normalize(viewPos - fragPosition);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * effectiveLight;

    // Attenuation (ligth distance amplification)
    float lightDistance = length(lightVector);
    float attenuation = 1.0 / (attenuationConstant + attenuationLinear * lightDistance
    + attenuationQuadratic * (lightDistance * lightDistance));
    diffuse *= attenuation;
    specular *= attenuation;

    vec3 color = (ambient + diffuse + specular) * albedo;

    // Post Processing Effects
    // ---------------------------------------------------------------

    // Inverse
    // color = 1.0 - texColor.rgb;

    // Greyscale
    // float average = 0.2126 * texColor.r + 0.7152 * texColor.g + 0.0722 * texColor.b;
    // color = vec3(average, average, average);

    // Kernel Effects

    // sharpening kernel
    //    const float kernel[9] = float[](
    //    -1, -1, -1,
    //    -1, 9, -1,
    //    -1, -1, -1
    //    );

    // bluring kernel
    //    const float kernel[9] = float[](
    //    1.0 / 16, 2.0 / 16, 1.0 / 16,
    //    2.0 / 16, 4.0 / 16, 2.0 / 16,
    //    1.0 / 16, 2.0 / 16, 1.0 / 16
    //    );

    // Edge detection kernel
    //    const float kernel[9] = float[](
    //    1, 1, 1,
    //    1, -8, 1,
    //    1, 1, 1
    //    );

    //    vec2 offset = vec2(1.0 / resolution.x, 1.0 / resolution.y);
    //    vec2 offsets[9] = vec2[](
    //    vec2(-offset.x, offset.y), // top-left
    //    vec2(0.0f, offset.y), // top-center
    //    vec2(offset.x, offset.y), // top-right
    //    vec2(-offset.x, 0.0f), // center-left
    //    vec2(0.0f, 0.0f), // center-center
    //    vec2(offset.x, 0.0f), // center-right
    //    vec2(-offset.x, -offset.y), // bottom-left
    //    vec2(0.0f, -offset.y), // bottom-center
    //    vec2(offset.x, -offset.y)  // bottom-right
    //    );
    //
    //    vec3 sampleTex[9];
    //    color = vec3(0.0);
    //
    //    for (int i = 0; i < 9; i++) {
    //        sampleTex[i] = vec3(texture(texture0, fragTexCoord + offsets[i]));
    //    }
    //    for (int i = 0; i < 9; i++) {
    //        color += sampleTex[i] * kernel[i];
    //    }

    // Output
    //    finalColor = vec4(color, texColor.a);

    // Output
    finalColor = vec4(color, albedoData.a);
}
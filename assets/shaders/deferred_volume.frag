#version 330 core

// Input from Vertex Shader
in vec3 lightCenterPos;

// Out to the LitScene Accumulation Buffer
out vec4 accumulatedLight;

// Uniforms
uniform vec2 resolution;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform float lightIntensity;
uniform float maxLightRadius;
uniform float attenuationConstant = 1.0;
uniform float attenuationLinear = 0.09;
uniform float attenuationQuadratic = 0.032;

// Toggles
uniform int enableGooch;
uniform int enableToon;

// G-Buffer
uniform sampler2D texture0;   // Albedo
uniform sampler2D gNormalTex; // Normal (RGB) + StyleID (Alpha)
uniform sampler2D gDepthTex;  // Hardware Depth

// Position Reconstruction
uniform mat4 invViewProj;

// global constants
const float specularStrength = 1.0;
const int shininess = 48;

void main()
{
    // Calculate Screen UV based on physical pixel coordinate
    vec2 screenUV = gl_FragCoord.xy / resolution;

    // Extract G-Buffer Data
    vec4 albedoData = texture(texture0, screenUV);
    vec3 albedo = albedoData.rgb;

    vec4 normalData = texture(gNormalTex, screenUV);
    vec3 rawNormal = normalData.rgb;
    int styleID = int(round(normalData.a));

    // Reconstruct World Position
    float depth = texture(gDepthTex, screenUV).r;

    // Skybox or unlit pixel (Light proxy sphere), abort lighting calculation
    if (depth == 1.0 && length(rawNormal) < 0.1) {
        discard;
    }

    vec4 ndc = vec4(screenUV.x * 2.0 - 1.0, screenUV.y * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldPos = invViewProj * ndc;
    vec3 fragPosition = worldPos.xyz / worldPos.w;

    // Software Culling: Discard pixels that fall outside the mathematical sphere radius
    vec3 lightVector = lightCenterPos - fragPosition;
    float lightDistance = length(lightVector);

    if (lightDistance > maxLightRadius) {
        discard;
    }

    // Shared Lighting Math
    vec3 normal = normalize(rawNormal);
    vec3 viewDir = normalize(viewPos - fragPosition);
    vec3 lightDir = normalize(lightVector);

    // Calculate Attenuation
    float attenuation = 1.0 / (attenuationConstant + attenuationLinear * lightDistance + attenuationQuadratic * (lightDistance * lightDistance));
    float distanceRatio = lightDistance / maxLightRadius;
    float distanceRatioQuad = distanceRatio * distanceRatio * distanceRatio * distanceRatio;
    attenuation *= clamp(1.0 - distanceRatioQuad, 0.0, 1.0);

    // BRDF EVALUATION & FALLBACK ROUTING
    // -----------------------------------------------------------------------

    bool useBlinnFallback = false;

    if (styleID == 2) {

        // GOOCH SHADING
        // -------------------------------------------------------------------------

        if (enableGooch == 1) {
            // Gooch Base Colors
            vec3 surfaceColor = vec3(0.8, 0.4, 0.1);
            vec3 coolColor = vec3(0.0, 0.0, 0.6) + 0.2 * surfaceColor;
            vec3 warmColor = vec3(0.6, 0.6, 0.0) + 0.6 * surfaceColor;

            // Gooch Diffuse Interpolation
            float t = (dot(normal, lightDir) + 1.0) / 2.0;
            vec3 goochDiffuse = mix(coolColor, warmColor, t);

            // Specular
            vec3 reflectDir = reflect(-lightDir, normal);
            float spec = pow(max(dot(viewDir, reflectDir), 0.0), 192.0); // 48 * 4
            vec3 specular = vec3(1.0) * spec;

            // Output Gooch evaluation
            vec3 resultingLight = (goochDiffuse + specular) * attenuation * lightIntensity;
            accumulatedLight = vec4(resultingLight, 1.0);
        } else {
            useBlinnFallback = true;
        }

    } else if (styleID == 3) {

        // TOON SHADING
        // -------------------------------------------------------------------------

        if (enableToon == 1) {
            // Toon Settings
            float levels = 8.0;
            vec3 toonBaseColor = vec3(1.0, 0.275, 0.333);

            // Diffuse
            float diff = max(dot(normal, lightDir), 0.0);

            // Quantization (Banding)
            float level = floor(diff * levels);
            diff = level / levels;
            vec3 diffuse = diff * lightColor * lightIntensity;

            // combine diffuse, attenuation and toon base color
            vec3 resultingLight = diffuse * attenuation;
            accumulatedLight = vec4(resultingLight * toonBaseColor, 1.0);
        } else {
            useBlinnFallback = true;
        }

    } else {
        // Route Style 1 (Blinn), Style 4 (Kuwahara base light), and invalid IDs to fallback
        useBlinnFallback = true;
    }

    // BASELINE BLINN-PHONG FALLBACK
    // -------------------------------------------------------------------------

    if (useBlinnFallback) {
        // Diffuse
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = diff * lightColor * lightIntensity;

        // Specular
        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
        vec3 specular = specularStrength * spec * lightColor * lightIntensity;

        // combine diffuse specular and attenuation
        vec3 resultingLight = (diffuse + specular) * attenuation;

        // Output light multiplied by albedo
        accumulatedLight = vec4(resultingLight * albedo, 1.0);
    }
}
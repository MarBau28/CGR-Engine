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
uniform int activeLights;
uniform float maxLightRadius;
uniform float attenuationConstant = 1.0;
uniform float attenuationLinear = 0.09;
uniform float attenuationQuadratic = 0.032;

// Toggles
uniform int enableOutlines;
uniform int enableKuwahara;
uniform int enableGooch;
uniform int enableToon;
uniform int kuwaharaRadius;
uniform float kuwaharaIntensity;

// G-Buffer
uniform sampler2D texture0;    // Albedo
uniform sampler2D gNormalTex;  // Normal (RGB) + StyleID (Alpha)
uniform sampler2D gDepthTex;   // Hardware Depth buffer

// Matrix for Position Reconstruction
uniform mat4 invViewProj;

// MRT Light Settings and Arrays
#define MAX_LIGHTS 500
uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColor;

// global constants
const float specularStrength = 1.0;
const int shininess = 48;

// Helper Blin-Phong
vec3 CalculateBlinnPhong(vec3 baseAlbedo, vec3 normal, vec3 fragPosition, vec3 viewDir) {
    vec3 totalLighting = vec3(1.0) * ambientLightStrength;

    for (int i = 0; i < activeLights; i++) {
        vec3 currentEffectiveLight = lightColor * lightIntensity;
        vec3 lightVector = lightPositions[i] - fragPosition;
        float lightDistance = length(lightVector);

        // If the light is to far away, ligth calc can be skipped
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
        float distanceRatioQuad = distanceRatio * distanceRatio
        * distanceRatio * distanceRatio;

        float windowing = clamp(1.0 - distanceRatioQuad, 0.0, 1.0);
        attenuation *= windowing;

        // Add this light's contribution to the total
        totalLighting += (diffuse + specular) * attenuation;
    }

    return totalLighting * baseAlbedo;
}

void main()
{
    // Extract data from G-Buffer
    vec4 albedoData = texture(texture0, fragTexCoord);
    vec3 albedo = albedoData.rgb;

    // Extract Normal and unpack StyleID from the alpha channel
    vec4 normalData = texture(gNormalTex, fragTexCoord);
    vec3 rawNormal = normalData.rgb;
    int styleID = int(round(normalData.a));

    // Reconstruct Worls Position from Depth
    float depth = texture(gDepthTex, fragTexCoord).r;

    // Map UV and Depth to Normalized Device Coordinates [-1, 1]
    vec4 ndc = vec4(
    fragTexCoord.x * 2.0 - 1.0,
    fragTexCoord.y * 2.0 - 1.0,
    depth * 2.0 - 1.0,
    1.0
    );

    // Unproject back to World Space
    vec4 worldPos = invViewProj * ndc;
    vec3 fragPosition = worldPos.xyz / worldPos.w;

    // Masking check (for light sources and sky box)
    if (length(rawNormal) < 0.1) {
        // If empty space draw backgroundColor
        if (albedoData.a == 0.0) {
            finalColor = vec4(backgroundColor, 1.0);
        }
        // If Alpha is > 0.0, it's normal object that should not be shaded (Light source)
        else {
            finalColor = vec4(albedo, 1.0);
        }
        return;
    }

    // Shared Vectors
    vec3 normal = normalize(rawNormal);
    vec3 viewDir = normalize(viewPos - fragPosition);
    bool useBlinnFallback = false;

    if (styleID == 2) {

        // GOOCH SHADING
        // -----------------------------------------------------------------------

        if (enableGooch == 1) {
            // Gooch Base Colors
            vec3 surfaceColor = vec3(0.8, 0.4, 0.1);
            vec3 coolColor = vec3(0.0, 0.0, 0.6) + 0.2 * surfaceColor;
            vec3 warmColor = vec3(0.6, 0.6, 0.0) + 0.6 * surfaceColor;

            vec3 totalLighting = coolColor * ambientLightStrength;

            for (int i = 0; i < activeLights; i++) {
                vec3 lightVector = lightPositions[i] - fragPosition;
                float lightDistance = length(lightVector);

                if (lightDistance > maxLightRadius) continue;

                vec3 lightDir = normalize(lightVector);

                // Gooch Diffuse Interpolation
                float t = (dot(normal, lightDir) + 1.0) / 2.0;
                vec3 goochDiffuse = mix(coolColor, warmColor, t);

                // Specular
                vec3 reflectDir = reflect(-lightDir, normal);
                float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess * 4.0);
                vec3 specular = vec3(1.0) * spec;

                // Attenuation
                float attenuation = 1.0 / (attenuationConstant + attenuationLinear
                * lightDistance + attenuationQuadratic * (lightDistance * lightDistance));
                float distanceRatio = lightDistance / maxLightRadius;
                float distanceRatioQuad = distanceRatio * distanceRatio * distanceRatio
                * distanceRatio;

                attenuation *= clamp(1.0 - distanceRatioQuad, 0.0, 1.0);
                totalLighting += (goochDiffuse + specular) * attenuation * lightIntensity;
            }

            finalColor = vec4(totalLighting, albedoData.a);
        } else {
            useBlinnFallback = true;
        }

    } else if (styleID == 3) {

        // SELECTIVE SOBEL OUTLINING & TOON
        // -------------------------------------------------------------------------

        bool isEdge = false;

        if (enableOutlines == 1) {
            vec2 texelSize = 1.0 / resolution;
            float edgeThickness = 2.0;

            // calculate dynamic outline thickness adjustments
            float dist = distance(viewPos, fragPosition);
            float maxFalloffDistance = 150.0;
            float distanceFactor = clamp(dist / maxFalloffDistance, 0.0, 1.0);
            float adaptiveThickness = mix(edgeThickness, 1.0, distanceFactor);

            // Sobel Kernel for X und Y
            float kernelX[9] = float[](
            1.0, 0.0, -1.0,
            2.0, 0.0, -2.0,
            1.0, 0.0, -1.0
            );
            float kernelY[9] = float[](
            1.0, 2.0, 1.0,
            0.0, 0.0, 0.0,
            -1.0, -2.0, -1.0
            );

            vec2 offsets[9] = vec2[](
            vec2(-1.0, 1.0), vec2(0.0, 1.0), vec2(1.0, 1.0),
            vec2(-1.0, 0.0), vec2(0.0, 0.0), vec2(1.0, 0.0),
            vec2(-1.0, -1.0), vec2(0.0, -1.0), vec2(1.0, -1.0)
            );

            vec3 normalEdgeX = vec3(0.0);
            vec3 normalEdgeY = vec3(0.0);

            for (int i = 0; i < 9; i++) {
                // Sampling of neigboring normals from G-Buffer
                vec2 offsetUV = fragTexCoord + offsets[i] * (texelSize * adaptiveThickness);
                vec3 sampleNormal = texture(gNormalTex, offsetUV).rgb;

                // Reconstruct neighbor position from Depth Buffer for edge detection
                float neighborDepth = texture(gDepthTex, offsetUV).r;
                vec4 neighborNdc = vec4(offsetUV.x * 2.0 - 1.0, offsetUV.y * 2.0 - 1.0, neighborDepth * 2.0 - 1.0, 1.0);
                vec4 neighborWorldPos = invViewProj * neighborNdc;
                vec3 neighborPos = neighborWorldPos.xyz / neighborWorldPos.w;
                float neighborDist = distance(viewPos, neighborPos);

                // Depth Masking (for overlaping objects)
                if (neighborDist < dist - 0.1) {
                    sampleNormal = rawNormal;
                }

                normalEdgeX += sampleNormal * kernelX[i];
                normalEdgeY += sampleNormal * kernelY[i];
            }

            // Calculate Gradient Magnitude
            float edge = sqrt(dot(normalEdgeX, normalEdgeX) + dot(normalEdgeY, normalEdgeY));
            float edgeThreshold = 0.1; // Thresholding for edges

            if (edge > edgeThreshold) {
                isEdge = true;
            }
        }

        if (isEdge) {
            finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        } else {
            if (enableToon == 1) {
                // TOON SHADING
                // -------------------------------------------------------------------

                vec3 totalToonLighting = vec3(0.0);
                vec3 toonBaseColor = vec3(1.0, 0.275, 0.333);
                float levels = 8.0;

                for (int i = 0; i < activeLights; i++) {
                    vec3 lightVector = lightPositions[i] - fragPosition;
                    float lightDistance = length(lightVector);

                    if (lightDistance > maxLightRadius) continue;

                    vec3 lightDir = normalize(lightVector);

                    // Diffuse
                    float diff = max(dot(normal, lightDir), 0.0);

                    // Quantization (Banding)
                    float level = floor(diff * levels);
                    diff = level / levels;
                    vec3 diffuse = diff * lightColor * lightIntensity;

                    // Attenuation
                    float attenuation = 1.0 / (attenuationConstant + attenuationLinear
                    * lightDistance + attenuationQuadratic
                    * (lightDistance * lightDistance));
                    float distanceRatio = lightDistance / maxLightRadius;
                    float distanceRatioQuad = distanceRatio * distanceRatio
                    * distanceRatio * distanceRatio;

                    attenuation *= clamp(1.0 - distanceRatioQuad, 0.0, 1.0);
                    totalToonLighting += diffuse * attenuation;
                }

                // Apply ambient and toon lighting
                vec3 ambient = ambientLightStrength * toonBaseColor;
                finalColor = vec4(ambient + (totalToonLighting * toonBaseColor), 1.0);
            } else {
                useBlinnFallback = true;
            }
        }

    } else if (styleID == 4) {

        // KUWAHARA FILTER
        // -----------------------------------------------------------------------

        if (enableKuwahara == 1) {
            vec2 texelSize = 1.0 / resolution;
            vec3 means[4];
            vec3 variances[4];

            for (int i = 0; i < 4; i++) {
                means[i] = vec3(0.0);
                variances[i] = vec3(0.0);
            }

            float samples = float((kuwaharaRadius + 1) * (kuwaharaRadius + 1));

            // Quadrant Offsets: Top-Left, Top-Right, Bottom-Left, Bottom-Right
            vec2 offsets[4] = vec2[](
            vec2(-1.0, -1.0), vec2(1.0, -1.0),
            vec2(-1.0, 1.0), vec2(1.0, 1.0)
            );

            // Calculate Mean and Variance for all 4 quadrants
            for (int i = 0; i < 4; i++) {
                vec3 sum = vec3(0.0);
                vec3 sumSq = vec3(0.0);

                for (int y = 0; y <= kuwaharaRadius; y++) {
                    for (int x = 0; x <= kuwaharaRadius; x++) {
                        vec2 offset = vec2(float(x) * offsets[i].x, float(y) * offsets[i].y);
                        vec3 col = texture(texture0, fragTexCoord + offset * texelSize).rgb;

                        sum += col;
                        sumSq += col * col;
                    }
                }

                means[i] = sum / samples;

                // Variance = E[X^2] - (E[X])^2
                variances[i] = abs(sumSq / samples - means[i] * means[i]);
            }

            // Generalized Kuwahara Weighted Sum
            float weightSum = 0.0;
            vec3 finalAlbedo = vec3(0.0);

            for (int i = 0; i < 4; i++) {
                float v = variances[i].r + variances[i].g + variances[i].b;
                // Epsilon 1e-4 prevents division by zero. Raise variance to the intensity power.
                float w = 1.0 / pow(abs(v) + 1e-4, kuwaharaIntensity);

                finalAlbedo += means[i] * w;
                weightSum += w;
            }

            finalAlbedo /= weightSum;

            //            // Find quadrant with the minimum variance
            //            float minVar = 1e6;
            //            vec3 finalAlbedo = vec3(0.0);
            //
            //            for (int i = 0; i < 4; i++) {
            //                // Sum of RGB variances
            //                float v = variances[i].r + variances[i].g + variances[i].b;
            //
            //                if (v < minVar) {
            //                    minVar = v;
            //                    finalAlbedo = means[i];
            //                }
            //            }

            // Saturation adn Tint boost for painting look
            float luma = dot(finalAlbedo, vec3(0.299, 0.587, 0.114));
            finalAlbedo = mix(vec3(luma), finalAlbedo, 1.8);
            finalAlbedo *= vec3(1.1, 1.0, 0.9);

            // Apply Standard Lighting to the abstracted "painterly" Albedo
            vec3 finalLitColor = CalculateBlinnPhong(finalAlbedo, normal, fragPosition, viewDir);
            finalColor = vec4(finalLitColor, 1.0);
        } else {
            useBlinnFallback = true;
        }

    } else {
        // styleID == 1 or Fallback for Debug
        useBlinnFallback = true;
    }

    // BASELINE FALLBACK EXECUTION
    // ---------------------------------------------------------------------------

    if (useBlinnFallback) {
        vec3 finalLitColor = CalculateBlinnPhong(albedo, normal, fragPosition, viewDir);
        finalColor = vec4(finalLitColor, albedoData.a);
    }
}
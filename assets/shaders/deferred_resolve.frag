#version 330 core

// Input
in vec2 fragTexCoord;

// Out to screen (or final Post-Process FBO)
out vec4 finalColor;

// Uniforms
uniform vec2 resolution;
uniform vec3 viewPos;
uniform vec3 backgroundColor;
uniform float ambientLightStrength;

// G-Buffer & Light Volume Inputs
uniform sampler2D litSceneTex; // Fully accumulated lighting from Phase Volume pass
uniform sampler2D texture0;    // Base Albedo
uniform sampler2D gNormalTex;  // Normal (RGB) + StyleID (Alpha)
uniform sampler2D gDepthTex;   // Hardware Depth buffer

// Matrix for Position Reconstruction
uniform mat4 invViewProj;

void main()
{
    // Extract Base Data (G-Buffer)
    vec4 albedoData = texture(texture0, fragTexCoord);
    vec3 albedo = albedoData.rgb;
    vec4 normalData = texture(gNormalTex, fragTexCoord);
    vec3 rawNormal = normalData.rgb;
    int styleID = int(round(normalData.a));

    // Extract Accumulated Lighting
    vec3 litColor = texture(litSceneTex, fragTexCoord).rgb;

    // Masking check for Skybox
    if (length(rawNormal) < 0.1) {
        if (albedoData.a == 0.0) {
            finalColor = vec4(backgroundColor, 1.0);
        } else {
            // Light source proxy spheres (unlit)
            finalColor = vec4(albedo, 1.0);
        }
        return;
    }

    // Reconstruct World Position
    float depth = texture(gDepthTex, fragTexCoord).r;
    vec4 ndc = vec4(fragTexCoord.x * 2.0 - 1.0, fragTexCoord.y * 2.0 - 1.0,
    depth * 2.0 - 1.0, 1.0);
    vec4 worldPos = invViewProj * ndc;
    vec3 fragPosition = worldPos.xyz / worldPos.w;

    // RESOLVE PASS EVALUATION
    // -----------------------------------------------------------------------

    if (styleID == 1) {

        // STANDARD BLINN-PHONG
        // ----------------------------------------------------------------------

        // Add Ambient Light to the accumulated Light Volume output
        vec3 ambient = albedo * ambientLightStrength;
        finalColor = vec4(litColor + ambient, albedoData.a);

    } else if (styleID == 2) {

        // GOOCH
        // ----------------------------------------------------------------------

        // no ambient for the Gooch shader
        finalColor = vec4(litColor, albedoData.a);

    } else if (styleID == 3) {

        // SELECTIVE SOBEL OUTLINING & TOON RESOLVE
        // ----------------------------------------------------------------------

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
        float edge = sqrt(dot(normalEdgeX, normalEdgeX)
                          + dot(normalEdgeY, normalEdgeY));
        float edgeThreshold = 0.1; // Thresholding for edges

        if (edge > edgeThreshold) {
            finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        } else {
            // Apply additional ambient to toon
            vec3 toonBaseColor = vec3(1.0, 0.275, 0.333);
            vec3 ambient = ambientLightStrength * toonBaseColor;
            finalColor = vec4(litColor + ambient, 1.0);
        }

    } else if (styleID == 4) {

        // KUWAHARA FILTER (Post-Lit)
        // -----------------------------------------------------------------------

        int radius = 4;
        vec2 texelSize = 1.0 / resolution;

        vec3 means[4];
        vec3 variances[4];

        for (int i = 0; i < 4; i++) {
            means[i] = vec3(0.0);
            variances[i] = vec3(0.0);
        }

        float samples = float((radius + 1) * (radius + 1));

        // Quadrant Offsets: Top-Left, Top-Right, Bottom-Left, Bottom-Rig
        vec2 offsets[4] = vec2[](
        vec2(-1.0, -1.0), vec2(1.0, -1.0),
        vec2(-1.0, 1.0), vec2(1.0, 1.0)
        );

        // Pre-calculate ambient to include in the variance sampling
        vec3 ambient = albedo * ambientLightStrength;

        // Calculate Mean and Variance for all 4 quadrants
        for (int i = 0; i < 4; i++) {
            vec3 sum = vec3(0.0);
            vec3 sumSq = vec3(0.0);

            for (int y = 0; y <= radius; y++) {
                for (int x = 0; x <= radius; x++) {
                    vec2 offset = vec2(float(x) * offsets[i].x, float(y) * offsets[i].y);
                    vec2 sampleUV = fragTexCoord + offset * texelSize;

                    // Sample the fully lit color + ambient for this pixel
                    vec3 sampledLit = texture(litSceneTex, sampleUV).rgb;
                    vec3 sampledAlbedo = texture(texture0, sampleUV).rgb;
                    vec3 sampledAmbient = sampledAlbedo * ambientLightStrength;

                    vec3 col = sampledLit + sampledAmbient;

                    sum += col;
                    sumSq += col * col;
                }
            }

            means[i] = sum / samples;

            // Variance = E[X^2] - (E[X])^2
            variances[i] = abs(sumSq / samples - means[i] * means[i]);
        }

        // Find quadrant with the minimum variance
        float minVar = 1e6;
        vec3 finalPainterlyColor = vec3(0.0);

        for (int i = 0; i < 4; i++) {
            // Sum of RGB variances
            float v = variances[i].r + variances[i].g + variances[i].b;
            if (v < minVar) {
                minVar = v;
                finalPainterlyColor = means[i];
            }
        }

        // Saturation and Tint boost for painting look
        float luma = dot(finalPainterlyColor, vec3(0.299, 0.587, 0.114));
        finalPainterlyColor = mix(vec3(luma), finalPainterlyColor, 1.8);
        finalPainterlyColor *= vec3(1.1, 1.0, 0.9);

        finalColor = vec4(finalPainterlyColor, 1.0);

    } else {
        // Fallback
        finalColor = vec4(1.0, 0.0, 1.0, 1.0);
    }
}
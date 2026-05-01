#version 330 core

layout (location = 0) in vec3 vertexPosition;
layout (location = 1) in vec2 vertexTexCoord;
layout (location = 2) in vec3 vertexNormal;
layout (location = 11) in mat4 instanceTransform;

uniform mat4 mvp;
uniform vec3 viewPos;

void main()
{
    // Reconstruct world position for distance calculation
    vec4 worldPos = instanceTransform * vec4(vertexPosition, 1.0);
    float dist = distance(viewPos, worldPos.xyz);

    // Dynamic thickness based on camera distance
    float maxFalloffDistance = 150.0;
    float distanceFactor = clamp(dist / maxFalloffDistance, 0.0, 1.0);

    // Increased base thickness for extreme visibility during testing
    float baseThickness = 0.20;
    float adaptiveThickness = mix(baseThickness, baseThickness * 0.4, distanceFactor);

    // Centroid Expansion: Scale outward from the local origin
    vec3 expandedPos = vertexPosition * (1.0 + adaptiveThickness);

    gl_Position = mvp * instanceTransform * vec4(expandedPos, 1.0);
}
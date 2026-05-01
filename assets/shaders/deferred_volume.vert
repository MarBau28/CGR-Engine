#version 330 core

// Vertex Attributes
layout (location = 0) in vec3 vertexPosition;

// In: Hardware Instanced Attributes
in mat4 instanceTransform;

// Raylib automatic MVP (Projection * View)
uniform mat4 mvp;

// Out to fragment shader
out vec3 lightCenterPos;

void main()
{
    // Extract the translation column from the 4x4 instance matrix
    lightCenterPos = vec3(instanceTransform[3][0], instanceTransform[3][1], instanceTransform[3][2]);

    // Calculate vertex position for rasterizer
    gl_Position = mvp * instanceTransform * vec4(vertexPosition, 1.0);
}
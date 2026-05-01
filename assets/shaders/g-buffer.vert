#version 330 core

// Input vertex attributes
layout (location = 0) in vec3 vertexPosition;
layout (location = 1) in vec2 vertexTexCoord;
layout (location = 2) in vec3 vertexNormal;

// Uniforms
uniform mat4 mvp;
uniform mat4 modelMat; // model matrix (world space transform)
uniform mat4 normalMat; // transforamtion matrix for normals to world space

// Output to fragment shader
out vec2 fragTexCoord;
out vec3 fragNormal;

void main()
{
    // Output the interpolated UV to the fragment shader
    fragTexCoord = vertexTexCoord;

    // Transform normal to world space using specialized normal matrix
    fragNormal = mat3(normalMat) * vertexNormal;

    // Multiply the local vertex by the MVP matrix to place it on screen
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
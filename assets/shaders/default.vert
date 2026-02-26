#version 330 core

layout (location = 0) in vec3 vertexPosition;
layout (location = 1) in vec2 vertexTexCoord;
//layout (location = 2) in vec3 vertexNormal;

// Model-View-Projection matrix
uniform mat4 mvp;

out vec2 fragTexCoord;

void main()
{
    // Output the interpolated UV to the fragment shader
    fragTexCoord = vertexTexCoord;

    // Multiply the local vertex by the MVP matrix to place it on screen
    gl_Position = mvp * vec4(vertexPosition, 1.0);

    //    gl_Position = vec4(aPos.x + aPos.x * scale, aPos.y + aPos.y * scale, aPos.z + aPos.z * scale, 1.0);
    //    color = aColor;
    //    texCoord = aTex;
}
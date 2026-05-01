#version 330 core

layout (location = 0) in vec3 vertexPosition;
layout (location = 1) in vec2 vertexTexCoord;
layout (location = 2) in vec3 vertexNormal;

uniform mat4 mvp;
uniform mat4 modelMat;
uniform mat4 normalMat;

out vec2 fragTexCoord;
out vec3 fragNormal;
out vec3 fragPosition;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragNormal = mat3(normalMat) * vertexNormal;

    vec4 worldPos = modelMat * vec4(vertexPosition, 1.0);
    fragPosition = worldPos.xyz;

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
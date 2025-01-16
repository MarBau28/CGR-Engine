#version 330 core

layout (location = 0) in vec3 vertexPosition;
layout (location = 1) in vec2 vertexTexCoord;
layout (location = 2) in vec3 vertexNormal;

// Hardware Instanced Attributes
in mat4 instanceTransform;

uniform mat4 mvp;

out vec2 fragTexCoord;
out vec3 fragNormal;
out vec3 fragPosition;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragNormal = mat3(instanceTransform) * vertexNormal;

    vec4 worldPos = instanceTransform * vec4(vertexPosition, 1.0);
    fragPosition = worldPos.xyz;

    gl_Position = mvp * worldPos;
}
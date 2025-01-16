#version 330 core

// Standard Vertex Attributes
layout (location = 0) in vec3 vertexPosition;
layout (location = 1) in vec2 vertexTexCoord;
layout (location = 2) in vec3 vertexNormal;
layout (location = 10) in float instanceStyleID;

// Hardware Instanced Attributes
in mat4 instanceTransform;

// Camera projection
uniform mat4 mvp;

// Out to fragment shader
out vec2 fragTexCoord;
out vec3 fragNormal;
out float fragStyleID;

void main()
{
    fragStyleID = instanceStyleID;
    fragTexCoord = vertexTexCoord;

    // Calculate World Position using instance matrix
    vec4 worldPos = instanceTransform * vec4(vertexPosition, 1.0);

    // Normal calculation
    fragNormal = mat3(instanceTransform) * vertexNormal;

    gl_Position = mvp * worldPos;
}
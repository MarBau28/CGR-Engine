#version 330 core

// in from vertex shader
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragPosition;

// MRT Outputs
layout (location = 0) out vec4 gAlbedo;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gPosition;

// uniforms
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform int isLightSource; // to identify unlit objects

void main()
{
    // Raw Color
    gAlbedo = texture(texture0, fragTexCoord) * colDiffuse;

    // Surface Normal
    if (isLightSource == 1) {
        // If it's light source, force empty normals to trigger the post-process mask
        gNormal = vec4(0.0);
    } else {
        gNormal = vec4(normalize(fragNormal), 1.0);
    }

    // World Position
    gPosition = vec4(fragPosition, 1.0);
}
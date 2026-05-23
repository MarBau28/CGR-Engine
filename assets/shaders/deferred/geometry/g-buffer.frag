#version 330 core

// in from vertex shader
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragPosition;

// MRT Outputs
layout (location = 0) out vec4 gAlbedo;
layout (location = 1) out vec4 gNormal;

// uniforms
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform int isLightSource; // to identify unlit objects
uniform float styleId; // Main Style ID for NPR Shader

void main()
{
    vec4 texColor = texture(texture0, fragTexCoord);

    // Alpha discard for masked textures
    if (texColor.a < 0.1) discard;

    // Raw Color
    gAlbedo = texture(texture0, fragTexCoord) * colDiffuse;

    vec3 packedNormal = normalize(fragNormal) * 0.5 + 0.5; // Pack Normal
    float packedStyle = styleId / 255.0; // Encode Style ID
    gNormal = vec4(packedNormal, packedStyle);
}
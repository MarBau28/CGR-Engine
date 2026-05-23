#version 330 core

in vec2 fragTexCoord;
in vec3 fragNormal;
in float fragStyleID;

// MRT Outputs
layout (location = 0) out vec4 gAlbedo;
layout (location = 1) out vec4 gNormal;

uniform sampler2D texture0;
uniform int isLightSource;

void main()
{
    vec4 texColor = texture(texture0, fragTexCoord);

    // Alpha discard for masked textures
    if (texColor.a < 0.1) discard;

    // If light sphere, render it white/unlit and bypass normal calculations
    if (isLightSource == 1) {
        gNormal = vec4(0.0);
        gAlbedo = vec4(1.0);
    } else {
        vec3 n = normalize(fragNormal);
        vec3 packedNormal = n * 0.5 + 0.5; // Pack Normal
        float packedStyle = fragStyleID / 255.0; // Encode Style ID
        gNormal = vec4(packedNormal, packedStyle);

        gAlbedo = texColor;
    }
}
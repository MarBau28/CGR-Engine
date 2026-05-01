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
    // If light sphere, render it white/unlit and bypass normal calculations
    if (isLightSource == 1) {
        gAlbedo = vec4(1.0);
        gNormal = vec4(0.0);
        return;
    }

    gAlbedo = texture(texture0, fragTexCoord);
    gNormal = vec4(normalize(fragNormal), fragStyleID);
}
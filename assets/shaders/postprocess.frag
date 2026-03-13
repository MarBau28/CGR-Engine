#version 330 core

// Input
in vec2 fragTexCoord;

// Out to screen
out vec4 finalColor;

// Uniforms
uniform sampler2D litSceneTex; // The fully lit image from lighting-pass
uniform vec2 resolution;

void main()
{
    // Fetch lit pixel
    vec4 pixColor = texture(litSceneTex, fragTexCoord);
    vec3 color = pixColor.rgb;

    // INVERSE COLORS EFFECT
    // ---------------------------------------------------------------
    //    color = 1.0 - pixColor.rgb;

    // GREYSCALE EFFECT
    // ---------------------------------------------------------------
    //    float average = 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
    //    color = vec3(average, average, average);

    // KERNEL EFFECTS
    // ---------------------------------------------------------------

    // SHARPENING KERNEL
    //    const float kernel[9] = float[](
    //    -1, -1, -1,
    //    -1, 9, -1,
    //    -1, -1, -1
    //    );

    // BLURING KERNEL
    //    const float kernel[9] = float[](
    //    1.0 / 16, 2.0 / 16, 1.0 / 16,
    //    2.0 / 16, 4.0 / 16, 2.0 / 16,
    //    1.0 / 16, 2.0 / 16, 1.0 / 16
    //    );

    // EDGE DETECTION KERNEL
    //    const float kernel[9] = float[](
    //    1, 1, 1,
    //    1, -8, 1,
    //    1, 1, 1
    //    );

    //    vec2 offset = vec2(1.0 / resolution.x, 1.0 / resolution.y);
    //    vec2 offsets[9] = vec2[](
    //    vec2(-offset.x, offset.y), // top-left
    //    vec2(0.0f, offset.y), // top-center
    //    vec2(offset.x, offset.y), // top-right
    //    vec2(-offset.x, 0.0f), // center-left
    //    vec2(0.0f, 0.0f), // center-center
    //    vec2(offset.x, 0.0f), // center-right
    //    vec2(-offset.x, -offset.y), // bottom-left
    //    vec2(0.0f, -offset.y), // bottom-center
    //    vec2(offset.x, -offset.y)  // bottom-right
    //    );
    //
    //    vec3 sampleTex[9];
    //    color = vec3(0.0);
    //
    //    for (int i = 0; i < 9; i++) {
    //        sampleTex[i] = vec3(texture(litSceneTex, fragTexCoord + offsets[i]));
    //    }
    //    for (int i = 0; i < 9; i++) {
    //        color += sampleTex[i] * kernel[i];
    //    }

    // Output
    finalColor = vec4(color, pixColor.a);
}
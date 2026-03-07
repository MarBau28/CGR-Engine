#version 330 core

// Input variables
in vec2 fragTexCoord;
in vec4 fragColor;

// out to screen
out vec4 finalColor;

// uniforms
uniform sampler2D texture0;
uniform vec2 resolution;

void main()
{
    // Texture
    vec4 texColor = texture(texture0, fragTexCoord);
    vec3 color = texColor.rgb;

    // Inverse
    // color = 1.0 - texColor.rgb;

    // Greyscale
    // float average = 0.2126 * texColor.r + 0.7152 * texColor.g + 0.0722 * texColor.b;
    // color = vec3(average, average, average);

    // Kernel Effects
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

    // sharpening kernel
    //    const float kernel[9] = float[](
    //    -1, -1, -1,
    //    -1, 9, -1,
    //    -1, -1, -1
    //    );

    // bluring kernel
    //    const float kernel[9] = float[](
    //    1.0 / 16, 2.0 / 16, 1.0 / 16,
    //    2.0 / 16, 4.0 / 16, 2.0 / 16,
    //    1.0 / 16, 2.0 / 16, 1.0 / 16
    //    );

    //Edge detection kernel
    //    const float kernel[9] = float[](
    //    1, 1, 1,
    //    1, -8, 1,
    //    1, 1, 1
    //    );

    //    vec3 sampleTex[9];
    //    color = vec3(0.0);
    //
    //    for (int i = 0; i < 9; i++) {
    //        sampleTex[i] = vec3(texture(texture0, fragTexCoord + offsets[i]));
    //    }
    //    for (int i = 0; i < 9; i++) {
    //        color += sampleTex[i] * kernel[i];
    //    }

    // Output
    finalColor = vec4(color, texColor.a);
}
#version 330 core

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;
uniform int viewMode; // 0 = Albedo, 1 = Normal, 2 = Position, 3 = StyleID

void main()
{
    vec4 texData = texture(texture0, fragTexCoord);

    if (viewMode == 0) {
        // Albedo (RGB as is)
        finalColor = vec4(texData.rgb, 1.0);
    }
    else if (viewMode == 1) {
        // Normals
        finalColor = vec4(normalize(texData.rgb) * 0.5 + 0.5, 1.0);
    }
    else if (viewMode == 2) {
        // Positions
        finalColor = vec4(texData.rgb * 0.05, 1.0);
    }
    else if (viewMode == 3) {
        // Style-ID (Packed in Alpha channel)
        int style = int(round(texData.a));

        if (style == 1) finalColor = vec4(1.0, 0.3, 0.3, 1.0); // Style 1 = Red
        else if (style == 2) finalColor = vec4(0.3, 0.3, 1.0, 1.0); // Style 2 = Blue
        else finalColor = vec4(0.1, 0.1, 0.1, 1.0); // Background/Lights/Floor 0 = Dark Grey
    }
}
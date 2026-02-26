#version 330 core

// in from vertex shader
in vec2 fragTexCoord;

// out to screen
out vec4 finalColor;

// Raylib's default texture sampler name
uniform sampler2D texture0;

void main()
{
    finalColor = texture(texture0, fragTexCoord);
}
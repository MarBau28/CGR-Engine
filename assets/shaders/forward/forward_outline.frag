#version 330 core
out vec4 finalColor;

void main()
{
    if (gl_FrontFacing) {
        discard;
    }

    finalColor = vec4(0.0, 0.0, 0.0, 1.0);
}
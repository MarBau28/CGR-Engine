#pragma once

#include "Shader.h"
#include <glad/glad.h>

class Texture {
public:
    GLuint ID{};
    GLenum type;

    Texture(const std::string &image, GLenum texType, GLenum slot, GLenum format, GLenum pixelType);

    static void TexUnit(const Shader &shader, GLuint unit);
    void Bind() const;
    void Unbind() const;
    void Delete() const;
};
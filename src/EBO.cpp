#include "EBO.h"

EBO::EBO(const GLuint* indices, const GLsizeiptr size) {
    glGenBuffers(1, &ID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indices, GL_STATIC_DRAW);
}

void EBO::Bind() const { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID); }

void EBO::Unbind() { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); }

void EBO::Delete() const { glDeleteBuffers(1, &ID); }
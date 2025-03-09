#include "VAO.h"

VAO::VAO() {
	glGenVertexArrays(1, &ID);
}

void VAO::LinkVBO(VBO VBO, GLuint layout) const {

	VBO.Bind();
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	VBO.Unbind();
}

void VAO::Bind() const {
	glBindVertexArray(ID);
}

void VAO::Unbind() const {
	glBindVertexArray(0);
}

void VAO::Delete() const {
	glDeleteVertexArrays(1, &ID);
}
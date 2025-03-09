#pragma once

#include<glad/glad.h>

class VBO {

	public: 
		GLuint ID;
		VBO(GLfloat* vertices, GLsizeiptr size);

		void Bind() const;
		void Unbind() const;
		void Delete() const;
};
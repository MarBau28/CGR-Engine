#pragma once

#include<glad/glad.h>

class VBO {

	public: 
		GLuint ID{};
		VBO(GLfloat* vertices, GLsizeiptr size);

		void Bind() const;
		static void Unbind();
		void Delete() const;
};
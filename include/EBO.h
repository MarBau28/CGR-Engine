#pragma once

#include<glad/glad.h>

class EBO {

	public:
		GLuint ID;
		EBO(GLuint* vindices, GLsizeiptr size);

		void Bind() const;
		void Unbind() const;
		void Delete() const;
};

#pragma once

#include<glad/glad.h>
#include"VBO.h"

class VAO {

	public:
		GLuint ID;
		VAO();

		void LinkVBO(VBO VBO, GLuint layout) const;
		void Bind() const;
		void Unbind() const;
		void Delete() const;
};


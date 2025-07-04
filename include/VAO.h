#pragma once

#include<glad/glad.h>
#include "VBO.h"

class VAO {

	public:
		GLuint ID{};
		VAO();

		static void LinkAttrib(const ::VBO& VBO, GLuint layout, GLint numComponents, GLenum type, GLsizei stride, const void* offset);
		void Bind() const;
		static void Unbind();
		void Delete() const;
};


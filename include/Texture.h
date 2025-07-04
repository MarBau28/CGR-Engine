#pragma once

#include<glad/glad.h>
#include "Shader.h"

class Texture
{
	public:
		GLuint ID{};
		GLenum type;
		Texture(const char* image, GLenum texType, GLenum slot, GLenum format, GLenum pixelType);

		static void texUnit(const Shader& shader, GLuint unit);
		void Bind() const;
		void Unbind() const;
		void Delete() const;
};
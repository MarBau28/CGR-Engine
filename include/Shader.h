#pragma once

#include<glad/glad.h>
#include<string>

std::string getFileContents(const char* filename);

class Shader {
	public:
		GLuint ID;
		Shader(const char* vertexFile, const char* fragmentFile);

		void ActivateProgram() const;
		void DeleteProgram() const;
};
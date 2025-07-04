#pragma once

#include<glad/glad.h>
#include<string>
#include<fstream>
#include<sstream>
#include<iostream>
#include<cerrno>

std::string getFileContents(const char* filename);

class Shader {
	public:
		GLuint ID;
		Shader(const char* vvertexFile, const char* fragmentFile);

		void ActivateProgram() const;
		void DeleteProgram() const;
};
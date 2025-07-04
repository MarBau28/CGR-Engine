#include "Shader.h"
#include <fstream>
#include <string>
#include <stdexcept>
#include <filesystem>

// file reader helper function
std::string getFileContents(const char* filename) {

	// Check if file exists and is a regular file.
	if (!std::filesystem::exists(filename)) {
		throw std::runtime_error("Error: File '" + std::string(filename) + "' does not exist.");
	}
	if (!std::filesystem::is_regular_file(filename)) {
		throw std::runtime_error("Error: '" + std::string(filename) + "' is not a regular file.");
	}

	// Open the file in binary mode.
	std::ifstream in(filename, std::ios::binary);

	// Check if the file was opened successfully.
	if (!in) {
		throw std::runtime_error("Error opening file '" + std::string(filename) + "'.");
	}

	std::string contents;
	in.seekg(0, std::ios::end);
	const std::streampos fileSize = in.tellg();

	// Check for valid file size.
	if (fileSize == static_cast<std::streampos>(-1)) {
		throw std::runtime_error("Error getting file size for '" + std::string(filename) + "'.");
	}

	contents.resize(static_cast<std::string::size_type>(fileSize));
	in.seekg(0, std::ios::beg);

	// Read content, avoiding narrowing conversion.
	in.read(contents.data(), static_cast<std::streamsize>(contents.size()));

	// Check if read operation was successful.
	if (!in) {
		throw std::runtime_error("Error reading contents from file '" + std::string(filename) + "'.");
	}

	in.close();
	return contents;
}

// shader constructor
Shader::Shader(const char* vertexFile, const char* fragmentFile) {

	// get shaders from source file
	const std::string vertexCode = getFileContents(vertexFile);
	const std::string fragmentCode = getFileContents(fragmentFile);
	const char* vertexSource = vertexCode.c_str();
	const char* fragmentSource = fragmentCode.c_str();

	// initialize and create vertex shader
	const GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, nullptr);
	glCompileShader(vertexShader);

	// initialize and create fragment shader
	const GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
	glCompileShader(fragmentShader);

	// attach shaders to openGL
	ID = glCreateProgram();
	glAttachShader(ID, vertexShader);
	glAttachShader(ID, fragmentShader);
	glLinkProgram(ID);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
}

void Shader::ActivateProgram() const {
	glUseProgram(ID);
}

void Shader::DeleteProgram() const {
	glDeleteProgram(ID);
}
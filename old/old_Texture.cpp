// #define STB_IMAGE_IMPLEMENTATION
//
// #include "old_Texture.h"
// #include "stb_image.h"
//
// Texture::Texture(const std::string &image, const GLenum texType, const GLenum slot, const GLenum format,
//                  const GLenum pixelType) {
//     type = texType;
//
//     int widthImg, heightImg, numColCh;
//     stbi_set_flip_vertically_on_load(true);
//     unsigned char *textureBytes = stbi_load(image.c_str(), &widthImg, &heightImg, &numColCh, 0);
//
//     glGenTextures(1, &ID);
//     glActiveTexture(slot);
//     glBindTexture(texType, ID);
//
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
//
//     glTexImage2D(texType, 0, GL_RGB, widthImg, heightImg, 0, format, pixelType, textureBytes);
//     glGenerateMipmap(texType);
//
//     stbi_image_free(textureBytes);
//     glBindTexture(texType, 0);
// }
//
// void Texture::TexUnit(const Shader &shader, GLuint unit) {
//     const GLint textureUniId = glGetUniformLocation(shader.ID, "tex0");
//     shader.ActivateProgram();
//     glUniform1i(textureUniId, 0);
// }
//
// void Texture::Bind() const { glBindTexture(type, ID); }
//
// void Texture::Unbind() const { glBindTexture(type, 0); }
//
// void Texture::Delete() const { glDeleteTextures(1, &ID); }

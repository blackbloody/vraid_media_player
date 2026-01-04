#ifndef TEXTURE_H
#define TEXTURE_H
#pragma once

#include "gl_glad_helper.h"
#include <string>
#include <vector>

struct MdlImageByte {
    int width;
    int height;
    int channels;
    std::vector<uint8_t> pixels; // RGBA8, etc.
};

enum class TextureFormat {
    R8,
    RGB8,
    RGBA8
};

class Texture {
private:
    GLuint rendererID;
    std::string file_path;
    unsigned char* localBuffer{};
    int width, height, bitPerPixel;

    GLint internalFormat;
    GLenum format;

    static void getGLFormat(TextureFormat fmt, GLint& internalFormat, GLenum& format) {
        switch (fmt) {
        case TextureFormat::R8:
            internalFormat = GL_R8;
            format = GL_RED;
            break;
        case TextureFormat::RGB8:
            internalFormat = GL_RGB8;
            format = GL_RGB;
            break;
        case TextureFormat::RGBA8:
            internalFormat = GL_RGBA8;
            format = GL_RGBA;
            break;
        }
    }

public:
    Texture(const std::string &file_path);
    Texture(int width, int height, const float* data);
    void updateText(int width, int height, const float *data);
    Texture(int width_pixel, int height_pixel, const std::vector<float>& tileXYZ);
    Texture(int width, int height, const std::vector<uint8_t>& data, int channel = 1);
    void updateText(int width, int height, const std::vector<uint8_t>& data, int channel = 1);
    ~Texture();

    void Bind(unsigned int slot = 0) const;
    void Unbind() const;

    inline int GetWidth() const { return width; }
    inline int GetHeight() const { return height; }

    static MdlImageByte loadImageByte(const std::string& filename, bool flipVertical = true);
};

#endif // TEXTURE_H

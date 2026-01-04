#include "texture.h"
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb_image.h>
#include <stb_image_write.h>

#include <cstdint>
#include <stdexcept>

Texture::Texture(const std::string &file_path) : rendererID(0), file_path(file_path), width(0), height(0), bitPerPixel(0) {

    stbi_set_flip_vertically_on_load(1);

    int rgba = STBI_rgb_alpha; // 4
    localBuffer = stbi_load(file_path.c_str(), &width, &height, &bitPerPixel, rgba);
    if (!localBuffer) {
        std::cerr << "Failed to load image: " << file_path << std::endl;
        return;
    }
    for (int i = 0; i < width * height * 4; i += 4) {
        unsigned char alpha = localBuffer[i + 3];
        if (alpha == 0) {
            localBuffer[i + 0] = 0; // R
            localBuffer[i + 1] = 255; // G
            localBuffer[i + 2] = 0; // B
            localBuffer[i + 3] = 255; // A (now fully opaque)
        }
    }

    GLCall(glGenTextures(1, &rendererID));
    GLCall(glBindTexture(GL_TEXTURE_2D, rendererID));

    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)); // x
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE)); // y

    GLCall(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, localBuffer));
    GLCall(glBindTexture(GL_TEXTURE_2D, 0));

    if (localBuffer) {
        stbi_image_free(localBuffer);
    }
}

Texture::Texture(int width, int height, const float *data) : rendererID(0), width(width), height(height), bitPerPixel(32) {

    GLCall(glGenTextures(1, &rendererID));
    GLCall(glBindTexture(GL_TEXTURE_2D, rendererID));

    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    // Upload the grayscale data as 32-bit float
    GLCall(glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_R32F,
        width,
        height,
        0,
        GL_RED,
        GL_FLOAT,
        data
        ));

    GLCall(glBindTexture(GL_TEXTURE_2D, 0));

}

void Texture::updateText(int width, int height, const float *data) {
    Bind();
    GLCall(glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_R32F,
        width,
        height,
        0,
        GL_RED,
        GL_FLOAT,
        data
        ));
    Unbind();
}

Texture::Texture(int width_pixel, int height_pixel, const std::vector<float>& tileXYZ) : rendererID(0), width(width_pixel), height(height_pixel), bitPerPixel(32) {

    GLCall(glGenTextures(1, &rendererID));
    GLCall(glBindTexture(GL_TEXTURE_2D, rendererID));

    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    // Extract only z values (1 per vertex)
    std::vector<float> tileZ;
    tileZ.reserve(width_pixel * height_pixel);
    for (size_t i = 0; i < tileXYZ.size(); i += 3) {
        tileZ.push_back(tileXYZ[i + 2]); // z value
    }

    // Upload the grayscale data as 32-bit float
    GLCall(glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_R32F,
        width,
        height,
        0,
        GL_RED,
        GL_FLOAT,
        tileZ.data()
        ));

    GLCall(glBindTexture(GL_TEXTURE_2D, 0));

}

Texture::Texture(int width, int height, const std::vector<uint8_t>& data, int channel) : rendererID(0), width(width), height(height) {

    TextureFormat fmt;
    if (channel == 1)      fmt = TextureFormat::R8;
    else if (channel == 3) fmt = TextureFormat::RGB8;
    else if (channel == 4) fmt = TextureFormat::RGBA8;

    GLint internalFormat;
    GLenum format;
    getGLFormat(fmt, internalFormat, format);

    this->internalFormat = internalFormat;
    this->format = format;

    GLCall(glGenTextures(1, &rendererID));
    GLCall(glBindTexture(GL_TEXTURE_2D, rendererID));

    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)); // preserve pixel edges
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    // GLCall(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    GLCall(glTexImage2D(
        GL_TEXTURE_2D,
        0,
        internalFormat,
        width,
        height,
        0,
        format,
        GL_UNSIGNED_BYTE,
        data.data()))
        ;
    // GLCall(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    GLCall(glBindTexture(GL_TEXTURE_2D, 0));
}

void Texture::updateText(int w, int h, const std::vector<uint8_t>& data, int channel)
{
    GLCall(glBindTexture(GL_TEXTURE_2D, rendererID));

    // (optional but safe)
    GLCall(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

    // Reallocate only if size changed
    if (w != width || h != height) {
        width = w;
        height = h;

        GLCall(glTexImage2D(
            GL_TEXTURE_2D,
            0,
            internalFormat,
            width,
            height,
            0,
            format,
            GL_UNSIGNED_BYTE,
            nullptr  // allocate only
            ));
    }

    // Upload pixels (no realloc)
    GLCall(glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0, 0,
        width,
        height,
        format,
        GL_UNSIGNED_BYTE,
        data.data()
        ));

    GLCall(glBindTexture(GL_TEXTURE_2D, 0));
}

/*
void Texture::updateText(int width, int height, const std::vector<uint8_t>& data, int channel) {

    Bind();

    GLCall(glTexImage2D(
        GL_TEXTURE_2D,
        0,
        internalFormat,
        width,
        height,
        0,
        format,
        GL_UNSIGNED_BYTE,
        data.data()))
        ;

    Unbind();
}
*/

Texture::~Texture() {
    GLCall(glDeleteTextures(1, &rendererID));
}

void Texture::Bind(unsigned int slot /*= 0*/) const {
    GLCall(glActiveTexture(GL_TEXTURE0 + slot));
    GLCall(glBindTexture(GL_TEXTURE_2D, rendererID));
}

void Texture::Unbind() const {
    GLCall(glBindTexture(GL_TEXTURE_2D, 0));
}

MdlImageByte Texture::loadImageByte(const std::string& filename, bool flipVertical) {
    if (flipVertical) {
        stbi_set_flip_vertically_on_load(1);
    }

    int w, h, n;
    const int desiredChannels = STBI_rgb_alpha;
    unsigned char* data = stbi_load(filename.c_str(), &w, &h, &n, desiredChannels);
    if (!data) {
        throw std::runtime_error(std::string("Failed to load image: ") + filename);
    }

    for (int i = 0; i < w * h * 4; i += 4) {
        unsigned char alpha = data[i + 3];
        if (alpha == 0) {
            data[i + 0] = 255; // R
            data[i + 1] = 255; // G
            data[i + 2] = 255; // B
            data[i + 3] = 255; // A (now fully opaque)
        }
    }

    n = desiredChannels;

    MdlImageByte res;
    res.width = w;
    res.height = h;
    res.channels = n;
    res.pixels.assign(data, data + (w * h * 4));

    stbi_image_free(data);

    return res;
}

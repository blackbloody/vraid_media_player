#ifndef INDEX_BUFFER_H
#define INDEX_BUFFER_H
#pragma once

#include <glad/glad.h>

class IndexBuffer {
private:
    GLuint rendererID;
    GLuint count;
public:
    IndexBuffer(const GLuint* data, GLuint count);
    ~IndexBuffer();

    void Bind() const;
    void Unbind() const;

    inline GLuint GetCount() const { return count; }
};

#endif // INDEX_BUFFER_H

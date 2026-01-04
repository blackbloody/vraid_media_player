#ifndef VERTEX_BUFFER_H
#define VERTEX_BUFFER_H

#pragma once
#include <glad/glad.h>
#include "gl_glad_helper.h"

class VertexBuffer {
private:
    GLuint rendererID;
public:
    VertexBuffer(const void* data, GLuint size, GLenum type=GL_STATIC_DRAW);
    ~VertexBuffer();

    void UpdateData(const void* data, GLuint size);

    void Bind() const;
    void Unbind() const;
};

#endif // VERTEX_BUFFER_H

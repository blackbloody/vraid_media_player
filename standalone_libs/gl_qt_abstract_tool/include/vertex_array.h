#ifndef VERTEX_ARRAY_H
#define VERTEX_ARRAY_H

#include <glad/glad.h>
#include "vertex_buffer.h"
#include "vertex_buffer_layout.h"

class VertexArray {
private:
    GLuint rendererID;
public:
    VertexArray();
    ~VertexArray();

    void AddBuffer(const VertexBuffer& vb, const VertexBufferLayout& layout) const;
    void Bind() const;
    void Unbind() const;
};

#endif // VERTEX_ARRAY_H

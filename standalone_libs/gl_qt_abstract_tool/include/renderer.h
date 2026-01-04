#ifndef RENDERER_H
#define RENDERER_H

#include "gl_glad_helper.h"
#include "vertex_array.h"
#include "index_buffer.h"
#include "shader.h"

class Renderer {
public:
    void Draw(const VertexArray& va, const IndexBuffer& ib, const Shader& shader, GLenum mode = GL_TRIANGLES);
    void DrawWithoutIndexBuffer(const VertexArray& va, const Shader& shader, const int &count, GLenum mode = GL_POINTS);
    void Clear() const;
};

#endif //RENDERER_H

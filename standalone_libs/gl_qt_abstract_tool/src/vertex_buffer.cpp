#include "gl_glad_helper.h"
#include "vertex_buffer.h"

VertexBuffer::VertexBuffer(const void* data, GLuint size, GLenum type) {
    GLCall(glGenBuffers(1, &rendererID));
    GLCall(glBindBuffer(GL_ARRAY_BUFFER, rendererID));
    GLCall(glBufferData(GL_ARRAY_BUFFER, size, data, type));
}


VertexBuffer::~VertexBuffer() {
    GLCall(glDeleteBuffers(1, &rendererID));
}

void VertexBuffer::UpdateData(const void* data, GLuint size) {
    Bind();

    // GLCall(glBufferSubData(GL_ARRAY_BUFFER, 0, size, data));

    // Option B (if size changes, or data completely replaces old)
    GLCall(glBufferData(GL_ARRAY_BUFFER, size, data, GL_DYNAMIC_DRAW));
}

void VertexBuffer::Bind() const {
    GLCall(glBindBuffer(GL_ARRAY_BUFFER, rendererID)); //
}

void VertexBuffer::Unbind() const {
    GLCall(glBindBuffer(GL_ARRAY_BUFFER, 0)); //
}

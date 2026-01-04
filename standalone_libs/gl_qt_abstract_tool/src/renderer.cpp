#include "renderer.h"

void Renderer::Draw(const VertexArray &va, const IndexBuffer &ib, const Shader &shader, GLenum mode) {
    shader.Bind();
    va.Bind();
    ib.Bind();
    GLCall(glDrawElements(mode, ib.GetCount(), GL_UNSIGNED_INT, nullptr));
}

void Renderer::DrawWithoutIndexBuffer(const VertexArray &va, const Shader &shader, const int &count, GLenum mode) {
    shader.Bind();
    va.Bind();
    GLCall(glDrawArrays(mode, 0, count));
}


void Renderer::Clear() const {
    GLCall(glClearColor(0.1f, 0.1f, 0.1f, 1.0f));
    GLCall(glClear(GL_COLOR_BUFFER_BIT));
}

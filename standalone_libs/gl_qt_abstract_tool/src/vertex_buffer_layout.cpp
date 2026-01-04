#include "vertex_buffer_layout.h"
#include "gl_glad_helper.h"

template<>
void VertexBufferLayout::Push<float>(unsigned int count) {
    std::cout << "Push buffer size: " << stride << std::endl;
    elements.push_back({GL_FLOAT, count, GL_FALSE});
    stride += count * VertexBufferElement::GetSizeOfType(GL_FLOAT);
}

template<>
void VertexBufferLayout::Push<GLuint>(unsigned int count) {
    std::cout << "Push buffer size: " << stride << std::endl;
    elements.push_back({GL_UNSIGNED_INT, count, GL_FALSE});
    stride += count * VertexBufferElement::GetSizeOfType(GL_UNSIGNED_INT);
}

template<>
void VertexBufferLayout::Push<GLbyte>(unsigned int count) {
    std::cout << "Push buffer size: " << stride << std::endl;
    elements.push_back({GL_UNSIGNED_BYTE, count, GL_FALSE});
    stride += count * VertexBufferElement::GetSizeOfType(GL_UNSIGNED_BYTE);
}

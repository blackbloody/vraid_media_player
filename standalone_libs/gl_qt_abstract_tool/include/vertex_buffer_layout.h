#ifndef VERTEX_BUFFER_LAYOUT_H
#define VERTEX_BUFFER_LAYOUT_H

#include <iostream>
#include <ostream>
#include <vector>
#include <glad/glad.h>

struct VertexBufferElement {
    GLuint type;
    GLuint count;
    GLboolean normalized;

    static unsigned int GetSizeOfType(GLenum type) {
        switch (type) {
        case GL_FLOAT: return 4;
        case GL_UNSIGNED_INT: return 4;
        case GL_UNSIGNED_BYTE: return 1;
        default: return 0;
        }
    }
};

class VertexBufferLayout {
private:
    std::vector<VertexBufferElement> elements;
    GLuint stride;
public:
    VertexBufferLayout() : stride(0) {
    }

    template<typename T>
    void Push(unsigned int count) {
        // static_assert(false);
        std::cerr << "Unsupported type for Push: " << typeid(T).name() << std::endl;
        static_assert(sizeof(T) != 0, "Unsupported type for Push");
    }

    void Pushs(unsigned int count) {
        elements.push_back({GL_FLOAT, count, GL_FALSE});
        stride += count * VertexBufferElement::GetSizeOfType(GL_FLOAT);
    }

    inline std::vector<VertexBufferElement> GetElements() const {return elements;}
    inline GLuint GetStride() const {return stride;}
};

#endif // VERTEX_BUFFER_LAYOUT_H

#include "gl_glad_helper.h"

#include <fstream>
#include <string>
#include <sstream>
#include <iostream>

#if defined(_MSC_VER)
#define DEBUG_BREAK() __debugbreak()
#else
#define DEBUG_BREAK() __builtin_trap()
#endif

#define ASSERT(x) if (!(x)) DEBUG_BREAK()
#define GLCall(x) GLClearError();\
x;\
    ASSERT(GlLogCall(#x, __FILE__, __LINE__))

    void GlGladHelper::GLClearError() {
    while (glGetError() != GL_NO_ERROR);
}

bool GlGladHelper::GlLogCall(const char *function, const char *file, int line) {
    while (GLenum error = glGetError()) {
        std::cerr << "OpenGL ERROR: " << "(" << error << ")" << " " << function << " " << file << " ~ " << line << std::endl;
        return false;
    }
    return true;
}

GLboolean GlGladHelper::check_error_glsl(const GLuint& shader, const std::string &shader_name) {
    int resultVs;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &resultVs);
    if (resultVs == GL_FALSE) {
        int length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        //std::vector<char> errorMessage(length + 1);
        char* message = (char*)alloca(length * sizeof(char));
        glGetShaderInfoLog(shader, length, &length, message);
        std::cerr << "Failed to compile " << shader_name << ": " <<message << std::endl;

        glDeleteShader(shader);

        return GL_FALSE;
    }
    return GL_TRUE;
}

GLuint GlGladHelper::compile_shader(const std::string& vertexShader, const std::string& fragmentShader) {
    GLuint program = glCreateProgram();

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    const char* srcVs = vertexShader.c_str();
    glShaderSource(vs, 1, &srcVs, nullptr);
    glCompileShader(vs);

    if (!check_error_glsl(vs, "GL_VERTEX_SHADER")) {
        return 0;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    const char* srcFs = fragmentShader.c_str();
    glShaderSource(fs, 1, &srcFs, nullptr);
    glCompileShader(fs);

    if (!check_error_glsl(fs, "GL_FRAGMENT_SHADER")) {
        return 0;
    }

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program); // IMPLEMENT PROGRAM
    glValidateProgram(program); // IMPLEMENT PROGRAM

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

GlGladHelper::ShaderSource GlGladHelper::load_shaders(const std::string& file_path) {
    std::ifstream stream(file_path);
    std::stringstream ss[2];

    enum class ShaderType {
        NONE = -1,
        VERTEX = 0,
        FRAGMENT = 1,
    };

    std::string line;
    ShaderType shaderType = ShaderType::NONE;
    while (getline(stream, line)) {
        if (line.find("#shader") != std::string::npos) {
            if (line.find("vertex") != std::string::npos)
                shaderType = ShaderType::VERTEX;
            else if (line.find("fragment") != std::string::npos)
                shaderType = ShaderType::FRAGMENT;
        } else {
            ss[(int)shaderType] << line << '\n';
        }
    }
    /*
    if (ss[0].str().empty()) {
        std::cerr << "Vertex shader source is empty!" << std::endl;
    }
    if (ss[1].str().empty()) {
        std::cerr << "Fragment shader source is empty!" << std::endl;
    }
    */

    return { ss[0].str(), ss[1].str() };
}

#ifndef GL_GLAD_HELPER_H
#define GL_GLAD_HELPER_H
#pragma once

#include <string>
#include <glad/glad.h>

#if defined(_MSC_VER)
#define DEBUG_BREAK() __debugbreak()
#else
#define DEBUG_BREAK() __builtin_trap()
#endif

#define ASSERT(x) if (!(x)) DEBUG_BREAK()
#define GLCall(x) GlGladHelper::GLClearError();\
x;\
    ASSERT(GlGladHelper::GlLogCall(#x, __FILE__, __LINE__))

class GlGladHelper {
public:
    struct ShaderSource {
        std::string vertex;
        std::string fragment;
    };
    static void GLClearError();
    static bool GlLogCall(const char* function, const char* file, int line);
    static GLboolean check_error_glsl(const GLuint& shader, const std::string &shader_name);
    static GLuint compile_shader(const std::string& vertexShader, const std::string& fragmentShader);
    static ShaderSource load_shaders(const std::string& file_path);
};

// ---- macros visible to every TU that includes this header ----
// #if defined(_MSC_VER)
// #define GL_DEBUG_BREAK() __debugbreak()
// #elif defined(__has_builtin) && __has_builtin(__builtin_trap)
// #define GL_DEBUG_BREAK() __builtin_trap()
// #else
// #include <csignal>
// #define GL_DEBUG_BREAK() raise(SIGTRAP)
// #endif

// #ifndef NDEBUG
// #define GL_ASSERT(x) do { if(!(x)) GL_DEBUG_BREAK(); } while(0)
// #define GLCall(x) do { \
// GlGladHelper::GLClearError(); \
//     (x); \
//     if (!GlGladHelper::GlLogCall(#x, __FILE__, __LINE__)) GL_DEBUG_BREAK(); \
// } while(0)
// #else
// #define GL_ASSERT(x) ((void)0)
// #define GLCall(x) (x)
// #endif

#endif // GL_GLAD_HELPER_H

#include "shader.h"
#include "gl_glad_helper.h"

Shader::Shader(const std::string &file_path) : filepath(file_path), program(0) {
    GlGladHelper::ShaderSource shader_source = GlGladHelper::load_shaders(file_path);

    // std::cout << file_path << std::endl;

    program = GlGladHelper::compile_shader(shader_source.vertex, shader_source.fragment);
}

Shader::~Shader() {
    GLCall(glDeleteProgram(program));
}

void Shader::SetUniform1i(const std::string& name, int value) {
    GLCall(glUniform1i(glGetUniformLocation(program, name.c_str()), value));
}

void Shader::SetUniform1f(const std::string &name, float value) {
    GLCall(glUniform1f(GetUniformLocation(name), value));
}

void Shader::SetUniform4f(const std::string &name, float v0, float v1, float v2, float v3) {
    GLCall(glUniform4f(GetUniformLocation(name), v0, v1, v2, v3));
}

void Shader::SetUniformMat4f(const std::string &name, const glm::mat4 &matrix) {
    GLCall(glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, &matrix[0][0]));
}

GLint Shader::GetUniformLocation(const std::string& name) {
    std::unordered_map<std::string, GLint>::const_iterator it = uniformsLocationCache.find(name);
    if (it != uniformsLocationCache.end())
        return it->second;

    GLint loc = -1;
#ifndef NDEBUG
    GLCall(loc = glGetUniformLocation(program, name.c_str())); // declare outside, assign inside
#else
    loc = glGetUniformLocation(program, name.c_str());
#endif

    if (loc == -1) {
        std::cout << "Shader uniform '" << name
                  << "' not found (possibly optimized out)\n";
    }

    uniformsLocationCache[name] = loc; // or insert/emplace if you prefer
    return loc;
}

void Shader::Bind() const {
    GLCall(glUseProgram(program));
}

void Shader::Unbind() const {
    GLCall(glUseProgram(0));
}

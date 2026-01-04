#ifndef SHADER_H
#define SHADER_H
#pragma once

#include <glad/glad.h>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <glm/glm.hpp>

class Shader {
private:
    std::string filepath;
    GLuint program;
    std::unordered_map<std::string, int> uniformsLocationCache;
public:
    Shader(const std::string& file_path);
    ~Shader();

    void Bind() const;
    void Unbind() const;

    // set uniform
    void SetUniform1i(const std::string& name, int value);
    void SetUniform1f(const std::string& name, float value);
    void SetUniform1iv(const std::string& name, const std::vector<int>& values);
    void SetUniform4f(const std::string& name, float v0, float v1, float v2, float v3);
    void SetUniformMat4f(const std::string& name, const glm::mat4& matrix);
private:
    // bool CompileShader();
    int GetUniformLocation(const std::string& name);
};

#endif // SHADER_H

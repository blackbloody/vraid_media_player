#shader vertex
#version 330 core

layout(location=0) in vec2 aPos;
layout(location=1) in vec4 aCol;
out vec4 vCol;
void main() { gl_Position = vec4(aPos, 0.0, 1.0); vCol = aCol; }

#shader fragment
#version 330 core

in vec4 vCol;
out vec4 FragColor;
void main() { FragColor = vCol; }
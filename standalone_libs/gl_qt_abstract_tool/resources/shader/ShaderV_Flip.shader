#shader vertex
#version 330 core

layout(location = 0) in vec2 aPos;      // 2D position
layout(location = 1) in vec2 aTexCoord;

out vec2 v_texCoord;

uniform mat4 u_MVP;

void main() {
    gl_Position = u_MVP * vec4(aPos, 0.0, 1.0);
    v_texCoord = aTexCoord;
}

#shader fragment
#version 330 core

layout(location = 0) out vec4 color;

in vec2 v_texCoord;

uniform sampler2D u_Texture;

void main() {
    //vec2 uv = vec2(v_texCoord.x, 1.0 - v_texCoord.y);
    vec2 uv = vec2(v_texCoord.x, v_texCoord.y);
    //vec4 uv = texture(u_Texture, v_texCoord);

    vec4 texColor = texture(u_Texture, uv);
    
    color = texColor;
}

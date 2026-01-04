#shader vertex
#version 330 core

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}

#shader fragment
#version 330 core

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_Texture;

// Viridis colormap lookup
vec3 viridis(float t) {
    const vec3 c0 = vec3(0.267004, 0.004874, 0.329415);
    const vec3 c1 = vec3(0.229739, 0.322361, 0.545706);
    const vec3 c2 = vec3(0.127568, 0.566949, 0.550556);
    const vec3 c3 = vec3(0.369214, 0.788888, 0.382914);
    const vec3 c4 = vec3(0.993248, 0.906157, 0.143936);

    // Piecewise linear interpolation
    if (t < 0.25)
        return mix(c0, c1, t / 0.25);
    else if (t < 0.50)
        return mix(c1, c2, (t - 0.25) / 0.25);
    else if (t < 0.75)
        return mix(c2, c3, (t - 0.50) / 0.25);
    else
        return mix(c3, c4, (t - 0.75) / 0.25);
}

vec3 inferno(float t) {
    const vec3 c0 = vec3(0.001462, 0.000466, 0.013866);
    const vec3 c1 = vec3(0.229739, 0.322361, 0.545706);
    const vec3 c2 = vec3(0.534998, 0.140453, 0.502256);
    const vec3 c3 = vec3(0.906311, 0.316313, 0.231078);
    const vec3 c4 = vec3(0.988362, 0.998364, 0.644924);

    if (t < 0.25)
        return mix(c0, c1, t / 0.25);
    else if (t < 0.50)
        return mix(c1, c2, (t - 0.25) / 0.25);
    else if (t < 0.75)
        return mix(c2, c3, (t - 0.50) / 0.25);
    else
        return mix(c3, c4, (t - 0.75) / 0.25);
}

vec3 magma(float t) {
    const vec3 c0 = vec3(0.001462, 0.000466, 0.013866);
    const vec3 c1 = vec3(0.18995,  0.07176, 0.23217);
    const vec3 c2 = vec3(0.38673,  0.12899, 0.41341);
    const vec3 c3 = vec3(0.63001,  0.21195, 0.55315);
    const vec3 c4 = vec3(0.987053, 0.991438, 0.749504);

    if (t < 0.25)
        return mix(c0, c1, t / 0.25);
    else if (t < 0.50)
        return mix(c1, c2, (t - 0.25) / 0.25);
    else if (t < 0.75)
        return mix(c2, c3, (t - 0.50) / 0.25);
    else
        return mix(c3, c4, (t - 0.75) / 0.25);
}

vec3 plasma(float t) {
    const vec3 c0 = vec3(0.050383, 0.029803, 0.527975);
    const vec3 c1 = vec3(0.516599, 0.005958, 0.644924);
    const vec3 c2 = vec3(0.826977, 0.278826, 0.447369);
    const vec3 c3 = vec3(0.995737, 0.681257, 0.304987);
    const vec3 c4 = vec3(0.940015, 0.975158, 0.131326);

    if (t < 0.25)
        return mix(c0, c1, t / 0.25);
    else if (t < 0.50)
        return mix(c1, c2, (t - 0.25) / 0.25);
    else if (t < 0.75)
        return mix(c2, c3, (t - 0.50) / 0.25);
    else
        return mix(c3, c4, (t - 0.75) / 0.25);
}

vec3 nipy_spectral(float t) {
    const vec3 c0 = vec3(0.0,     0.0,     0.5);    // Dark blue
    const vec3 c1 = vec3(0.0,     0.0,     1.0);    // Blue
    const vec3 c2 = vec3(0.0,     1.0,     1.0);    // Cyan
    const vec3 c3 = vec3(0.0,     1.0,     0.0);    // Green
    const vec3 c4 = vec3(1.0,     1.0,     0.0);    // Yellow
    const vec3 c5 = vec3(1.0,     0.0,     0.0);    // Red
    const vec3 c6 = vec3(0.5,     0.0,     0.0);    // Dark red

    if (t < 0.17)
        return mix(c0, c1, t / 0.17);
    else if (t < 0.34)
        return mix(c1, c2, (t - 0.17) / 0.17);
    else if (t < 0.50)
        return mix(c2, c3, (t - 0.34) / 0.16);
    else if (t < 0.67)
        return mix(c3, c4, (t - 0.50) / 0.17);
    else if (t < 0.84)
        return mix(c4, c5, (t - 0.67) / 0.17);
    else
        return mix(c5, c6, (t - 0.84) / 0.16);
}

vec3 ocean(float t) {
    // Clamp t to [0,1] just to be safe
    t = clamp(t, 0.0, 1.0);

    float r = 0.0;
    float g = 0.5 * t;
    float b = 0.5 + 0.5 * t;

    return vec3(r, g, b);
}

uniform int u_Colormap;

void main() {
    float intensity = texture(u_Texture, v_TexCoord).r;
    vec3 color;

    if (u_Colormap == 1) {
        color = viridis(intensity);
    }
    else if (u_Colormap == 2) {
        color = inferno(intensity);
    }
    else if (u_Colormap == 3) {
        color = magma(intensity);
    }
    else if (u_Colormap == 4) {
        color = plasma(intensity);
    }
    else if (u_Colormap == 5) {
        color = nipy_spectral(intensity);
    }
    else if (u_Colormap == 6) {
        color = ocean(intensity);
    }
    else {
        color = vec3(intensity);
    }
    FragColor = vec4(color, 1.0);
}
#version 330 core

layout(location = 0) in vec3 a;
layout(location = 2) in vec2 t;

out vec2 uv;
out vec3 p;

uniform mat4 m;
uniform mat4 v;
uniform mat4 pj;

void main()
{
    p = vec3(m * vec4(a, 1.0));
    uv = t;
    gl_Position = pj * v * m * vec4(a, 1.0);
}
#version 330 core

in vec2 uv;
in vec3 p;

out vec4 f;

uniform vec3 tint;
uniform vec3 lPos;
uniform sampler2D tex;

void main()
{
    vec3 c = texture(tex, uv).rgb * tint;
    f = vec4(0.3 * c + max(dot(vec3(0,1,0), normalize(lPos - p)), 0.0) * c, 1.0);
}
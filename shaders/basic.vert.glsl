#version 430 core

layout(location=0) in vec3 pos;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;

uniform mat4 viewproj;
uniform mat4 model;
uniform mat4 normalMatrix;

out vec3 n;

void main() {
    gl_Position = viewproj * model * vec4(pos, 1);
    n = normalize(mat3(normalMatrix) * normal);
}
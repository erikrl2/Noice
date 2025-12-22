#version 430 core

layout(location=0) in vec3 pos;
layout(location=1) in vec3 tangent;

out vec3 t;

uniform mat4 viewproj;
uniform mat4 model;
uniform mat4 normalMatrix;

void main() {
    gl_Position = viewproj * model * vec4(pos, 1);
    t = mat3(normalMatrix) * tangent;
}
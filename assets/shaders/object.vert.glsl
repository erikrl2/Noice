#version 430 core

layout(location=0) in vec3 pos;
layout(location=1) in vec3 tangent;

uniform mat4 model;
uniform mat4 viewproj;

out VS_OUT {
    vec3 pWorld;
    vec3 tWorld;
} vs_out;

void main() {
    vs_out.pWorld = (model * vec4(pos, 1.0)).xyz;
    vs_out.tWorld = normalize(mat3(model) * tangent);

    gl_Position = viewproj * vec4(vs_out.pWorld, 1.0);
}

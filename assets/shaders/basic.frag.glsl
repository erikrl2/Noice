#version 430 core

in vec3 t;

layout(location=0) out vec2 outRG;

uniform mat4 viewproj;

void main() {
    outRG = normalize(viewproj * vec4(t, 0)).xy;
}

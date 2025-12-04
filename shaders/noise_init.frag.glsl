#version 330 core

in vec2 uv;

out vec4 FragColor;

float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    float v = rand(uv * 2024.0);
    FragColor = vec4(vec3(v), 1);
}

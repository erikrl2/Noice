#version 430 core

out vec4 FragColor;

float rand() {
    return fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    FragColor = vec4(vec3(step(0.5, rand())), 1.0);
}


#version 430 core

out vec4 FragColor;

uniform vec2 flowDir;

void main() {
    FragColor = vec4(normalize(flowDir), 0, 0);
}
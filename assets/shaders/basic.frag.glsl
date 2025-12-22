#version 430 core

in vec3 t;

out vec4 FragColor;

uniform mat4 viewproj;

void main() {
    FragColor = vec4(normalize(viewproj * vec4(t, 0)).xy, 0, 0);
}
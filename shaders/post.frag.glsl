#version 330 core

in vec2 uv;

out vec4 FragColor;

uniform sampler2D screenTex;

void main() {
    vec3 col = texture(screenTex, uv).rgb;
    FragColor = vec4(col, 1);
}

#version 330 core

in vec2 uv;

out vec4 FragColor;

uniform sampler2D prevTex;
uniform sampler2D objectTex;

void main() {
    vec3 a = texture(prevTex, uv).rgb;
    vec3 b = texture(objectTex, uv).rgb;

    FragColor = vec4(abs(a - b), 1);
}

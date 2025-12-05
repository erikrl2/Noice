#version 330 core

out vec4 FragColor;

float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 p = floor(gl_FragCoord.xy);
    float b = step(0.5, rand(p));
    FragColor = vec4(vec3(b), 1.0);
}
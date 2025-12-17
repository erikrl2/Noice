#version 430 core

out vec4 FragColor;

uniform sampler2D screenTex;
uniform vec2 resolution;
uniform bool showFlow;

void main() {
    vec2 v = texture(screenTex, gl_FragCoord.xy / resolution).rg;
    FragColor = !showFlow ? vec4(vec3(v.r), 1) : vec4(v * 0.5 + 0.5, 0, 1);
}

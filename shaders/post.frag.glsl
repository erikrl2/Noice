#version 430 core

out vec4 FragColor;

uniform sampler2D screenTex;
uniform vec2 resolution;

void main() {
    vec3 col = texture(screenTex, gl_FragCoord.xy / resolution).rgb;
    FragColor = vec4(col, 1);
}

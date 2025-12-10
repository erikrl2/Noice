#version 420 core

out vec4 FragColor;

uniform sampler2D screenTex;
uniform vec2 resolution;

void main() {
    vec2 uv = gl_FragCoord.xy / resolution;
    vec3 col = texture(screenTex, uv).rgb;
    FragColor = vec4(col, 1);
}

#version 430 core
out vec4 FragColor;
uniform sampler2D screenTex;
uniform vec2 resolution;
void main() {
#if 1
    FragColor = texture(screenTex, gl_FragCoord.xy / resolution);
#else
    vec2 v = texture(screenTex, gl_FragCoord.xy / resolution).rg;
    FragColor = vec4(v * 0.5 + 0.5, 0, 1);
#endif
}
#version 430 core

in vec2 uv;

layout(location=0) out vec2 outRG;

uniform sampler2D fontAtlas;

uniform vec2 dir;

void main() {
    float a = texture(fontAtlas, uv).r;
    float cov = smoothstep(0.4, 0.6, a);

    if (cov <= 0.0)
        discard;

    outRG = dir * cov;
}

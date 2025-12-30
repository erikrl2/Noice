#version 430 core

layout(location = 0) out vec2 oDir;

uniform sampler2D uCanvas;

vec2 safeNorm(vec2 v) {
    float l = length(v);
    if (l < 1e-6) return vec2(0.0);
    return v / l;
}

void main() {
    vec2 size = vec2(textureSize(uCanvas, 0));
    vec2 vUV = gl_FragCoord.xy / size;
    vec4 c = texture(uCanvas, vUV);

    if (c.a <= 1e-5) {
        oDir = vec2(0.0);
        return;
    }

    vec2 dir = c.xy / max(c.a, 1e-6);
    dir = safeNorm(dir);

    dir.y = -dir.y;
    oDir = dir;
}

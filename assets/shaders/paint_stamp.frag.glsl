#version 430 core

out vec4 o;

uniform vec2 uCenterPx; // pixel coords, origin top-left
uniform float uRadiusPx;
uniform vec2 uDir; // normalized direction in [-1,1]

uniform sampler2D uCanvas;

uniform float uSoftness;
uniform float uStrength;

vec2 uvToPx(vec2 uv, vec2 size) {
    return vec2(uv.x * size.x, (1.0 - uv.y) * size.y);
}

void main() {
    vec2 size = vec2(textureSize(uCanvas, 0));
    vec2 vUV = gl_FragCoord.xy / size;
    vec2 px = uvToPx(vUV, size);
    float r = length(px - uCenterPx);

    float edge0 = uRadiusPx * (1.0 - uSoftness);
    float edge1 = uRadiusPx;
    float a = 1.0 - smoothstep(edge0, edge1, r);
    float w = a * uStrength;

    vec4 prev = texture(uCanvas, vUV); // xy = acc dir sum, a = acc weight

    vec2 sum = prev.xy + uDir * w;
    float W  = prev.a  + w;

    o = vec4(sum, 0.0, W);
}

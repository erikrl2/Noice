#version 430 core

out vec2 oDir;

uniform vec2 uCenter;
uniform float uRadius;
uniform vec2 uDir;

uniform sampler2D uCanvas;

void main() {
    vec2 size = vec2(textureSize(uCanvas, 0));

    vec2 pos = vec2(gl_FragCoord.x, size.y - gl_FragCoord.y);

    float dist = length(pos - uCenter);
    float weight = 0.55 * (1.0 - smoothstep(uRadius * 0.4, uRadius, dist));

    vec2 prevDir = texelFetch(uCanvas, ivec2(gl_FragCoord.xy), 0).xy;

    oDir = prevDir + uDir * weight;
}

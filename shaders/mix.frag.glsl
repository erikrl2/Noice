#version 330 core

out vec4 FragColor;

uniform sampler2D prevTex;
uniform sampler2D objectTex;

uniform float doXor;
uniform vec2 scrollDir;
uniform float rand;

float rng() {
    ivec2 p = ivec2(gl_FragCoord.xy);
    uint x = uint(p.x);
    uint y = uint(p.y);
    uint seed = floatBitsToUint(rand);
    uint n = x * 1664525u + y * 1013904223u + seed * 1103515245u;
    n ^= (n >> 16);
    n *= 0x45d9f3bdu;
    n ^= (n >> 16);
    return float(n) * 2.3283064365386963e-10;
}

void main() {
    vec2 texSize = vec2(textureSize(prevTex, 0));
    vec2 uv = gl_FragCoord.xy / texSize;
    vec3 objColor = texture(objectTex, uv).rgb;

    if (objColor != vec3(1, 0, 0)) { // flicker wireframe
        vec3 noiseColor = texture(prevTex, uv).rgb;
        FragColor = vec4(doXor == 1.0 ? abs(objColor - noiseColor) : noiseColor, 1);
    }
    else { // scroll faces
        vec2 nextUV = (gl_FragCoord.xy - scrollDir) / texSize;

        if (texture(objectTex, nextUV).rgb != vec3(1, 0, 0)) {
            FragColor = vec4(vec3(step(0.5, rng())), 1);
        } else {
            FragColor = vec4(texture(prevTex, nextUV).rgb, 1);
        }
    }
}


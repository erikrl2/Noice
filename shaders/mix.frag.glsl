#version 430 core

out vec4 FragColor;

uniform sampler2D prevTex;
uniform sampler2D objectTex;

uniform float doXor;
uniform vec2 scrollOffset;
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
    ivec2 texSize = textureSize(prevTex, 0);
    ivec2 fragPx = ivec2(gl_FragCoord.xy);

    vec3 objColor = texture(objectTex, gl_FragCoord.xy / vec2(texSize)).rgb;
    if (objColor != vec3(1, 0, 0)) { // xor (flickers wireframe)
        vec3 noiseColor = texelFetch(prevTex, fragPx, 0).rgb;
        FragColor = vec4(doXor == 1.0 ? abs(objColor - noiseColor) : noiseColor, 1);
    }
    else { // scroll faces
        ivec2 off = ivec2(scrollOffset);
        ivec2 nextPx = fragPx - off;
        ivec2 wrappedPx = ivec2(((nextPx.x % texSize.x) + texSize.x) % texSize.x, ((nextPx.y % texSize.y) + texSize.y) % texSize.y);

        vec3 objColorAtNext = texture(objectTex, (vec2(nextPx) + 0.5) / vec2(texSize)).rgb;
        if (objColorAtNext != vec3(1, 0, 0)) {
            FragColor = vec4(vec3(step(0.5, rng())), 1);
        } else {
            FragColor = vec4(texelFetch(prevTex, wrappedPx, 0).rgb, 1);
        }
    }
}

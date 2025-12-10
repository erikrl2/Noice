#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba8, binding = 0) uniform image2D noiseTex;

uniform sampler2D objectTex;
uniform float rand;

uint hash(uint x) {
    x ^= x >> 16;
    x *= 0x45d9f3bdu;
    x ^= x >> 16;
    return x;
}

float rng(ivec2 px, uint seed) {
    uint x = uint(px.x);
    uint y = uint(px.y);
    uint n = x * 1664525u + y * 1013904223u + seed * 1103515245u;
    return float(hash(n)) * 2.3283064365386963e-10;
}

void main() {
    ivec2 px = ivec2(gl_GlobalInvocationID.xy);
    ivec2 noiseSize = imageSize(noiseTex);
    
    if (px.x >= noiseSize.x || px.y >= noiseSize.y) {
        return;
    }
    
    vec4 currentValue = imageLoad(noiseTex, px);
    
    // NUR wenn Pixel leer ist (Alpha < 0.5)
    if (currentValue.a < 0.5) {
        // Generiere neues Noise
        uint seed = floatBitsToUint(rand);
        float r = rng(px, seed);
        vec3 newNoise = vec3(step(0.5, r));
        imageStore(noiseTex, px, vec4(newNoise, 1.0));
    }
    // SONST: NICHTS tun! Pixel bleibt unverändert
}

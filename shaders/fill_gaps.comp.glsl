#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba8, binding = 0) uniform image2D noiseTex;

uniform float rand;

uint hash(uint x) {
    x ^= x >> 16;
    x *= 0x45d9f3bdu;
    x ^= x >> 16;
    return x;
}

float rng(ivec2 px, float seed) {
    uint x = uint(px.x);
    uint y = uint(px.y);
    uint s = floatBitsToUint(seed);
    uint n = x * 1664525u + y * 1013904223u + s * 1103515245u;
    n = hash(n);
    return float(n) * 2.3283064365386963e-10;
}

void main() {
    ivec2 px = ivec2(gl_GlobalInvocationID.xy);
    ivec2 noiseSize = imageSize(noiseTex);
    
    if (px.x >= noiseSize.x || px.y >= noiseSize.y) {
        return;
    }
    
    if (imageLoad(noiseTex, px).a < 0.5) {
        vec3 newNoise = vec3(step(0.5, rng(px, rand)));
        imageStore(noiseTex, px, vec4(newNoise, 1.0));
    }
}
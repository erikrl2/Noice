#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rg8, binding = 0) uniform image2D currNoiseTex;
layout(rg16f, binding = 1) uniform image2D currAccTex;
layout(rg16f, binding = 2) uniform readonly image2D prevAccTex;

uniform float rand;

float rng(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    ivec2 px = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(currNoiseTex);
    if (px.x >= size.x || px.y >= size.y) return;

    float hasVal = imageLoad(currNoiseTex, px).g;
    if (hasVal < 0.1) {
        float noiseVal = step(0.5, rng(vec2(px) + rand));
        imageStore(currNoiseTex, px, vec4(noiseVal, 1, 0, 0));

        vec2 inheritedAcc = imageLoad(prevAccTex, px).xy;
        imageStore(currAccTex, px, vec4(inheritedAcc, 0, 0));
    }
}


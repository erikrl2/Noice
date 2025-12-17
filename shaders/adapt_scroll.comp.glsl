#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rg8, binding = 0) uniform writeonly image2D currNoiseTex;
layout(rg16f, binding = 1) uniform writeonly image2D currAccTex;
layout(rg16f, binding = 2) uniform image2D prevAccTex;

uniform sampler2D prevNoiseTex;
uniform sampler2D flowTex;
uniform sampler2D depthTex;

uniform float scrollSpeed;

void main() {
    ivec2 size = textureSize(prevNoiseTex, 0);
    ivec2 prevPx = ivec2(gl_GlobalInvocationID.xy);
    if (prevPx.x >= size.x || prevPx.y >= size.y) return;

    vec2 noise = texelFetch(prevNoiseTex, prevPx, 0).rg;
    if (noise.g < 0.1) return;

    vec2 uv = (vec2(prevPx) + 0.5) / vec2(size);
    float depth = texture(depthTex, uv).r;
    if (depth >= 1.0) {
        imageStore(currNoiseTex, prevPx, vec4(noise.r, 1, 0, 0));
        return;
    }

    vec2 flowDir = normalize(texture(flowTex, uv).xy);
    vec2 flow = flowDir * scrollSpeed;

    vec2 prevAcc = imageLoad(prevAccTex, prevPx).xy;
    vec2 totalMove = prevAcc + flow;
    
    vec2 intStep = trunc(totalMove);
    vec2 nextAcc = totalMove - intStep;

    imageStore(prevAccTex, prevPx, vec4(nextAcc, 0, 0));

    ivec2 targetPx = prevPx + ivec2(intStep);

    if (targetPx.x >= 0 && targetPx.x < size.x && targetPx.y >= 0 && targetPx.y < size.y) {
        imageStore(currNoiseTex, targetPx, vec4(noise.r, 1, 0, 0));
        imageStore(currAccTex, targetPx, vec4(nextAcc, 0, 0));
    }
}

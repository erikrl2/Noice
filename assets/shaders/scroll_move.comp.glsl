#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rg8, binding = 0) uniform writeonly image2D currNoiseTex;
layout(rg8, binding = 1) uniform readonly image2D prevNoiseTex;
layout(rg16f, binding = 2) uniform writeonly image2D currAccTex;
layout(rg16f, binding = 3) uniform image2D prevAccTex;

uniform sampler2D flowTex;
uniform sampler2D currDepthTex;
uniform sampler2D prevDepthTex;

uniform mat4 invPrevProj;
uniform mat4 invPrevView;
uniform mat4 invPrevModel;
uniform mat4 currModel;
uniform mat4 currViewProj;

uniform float scrollSpeed;
uniform bool reproject;

void main() {
    ivec2 prevPx = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(prevNoiseTex);
    if (prevPx.x >= size.x || prevPx.y >= size.y) return;

    vec2 prevNoise = imageLoad(prevNoiseTex, prevPx).rg;
    if (prevNoise.g < 0.1) return;

    vec2 prevUV = (vec2(prevPx) + 0.5) / vec2(size);
    
    vec2 currUV;
    ivec2 currPx;
    vec3 currNDC;

    if (reproject) {
        float prevDepth = texture(prevDepthTex, prevUV).r;
        if (prevDepth >= 1.0) {
            imageStore(currNoiseTex, prevPx, vec4(prevNoise.r, 1, 0, 0));
            return;
        }

        vec3 prevNDC = vec3(prevUV, prevDepth) * 2.0 - 1.0;
        vec4 prevClip = vec4(prevNDC, 1.0);
        vec4 prevViewPos = invPrevProj * prevClip;
        prevViewPos /= prevViewPos.w;
        vec4 prevWorldPos = invPrevView * vec4(prevViewPos.xyz, 1.0);

        vec4 localPos = invPrevModel * prevWorldPos;
        vec4 currWorldPos = currModel * localPos;
        vec4 currClip = currViewProj * currWorldPos;

        if (currClip.w <= 0.0) return;

        currNDC = currClip.xyz / currClip.w;

        if (abs(currNDC.x) > 1.0 || abs(currNDC.y) > 1.0 || abs(currNDC.z) > 1.0) return;

        currUV = currNDC.xy * 0.5 + 0.5;
        currPx = ivec2(currUV * vec2(size));
    } else {
        currUV = prevUV;
        currPx = prevPx;
    }


    vec2 flowDir = texture(flowTex, currUV).xy;
    vec2 prevAcc = imageLoad(prevAccTex, prevPx).xy;

    vec2 flow = flowDir * scrollSpeed;

    vec2 totalMove = prevAcc + flow;
    vec2 intStep = trunc(totalMove); // or floor(totalMove + 0.5 * sign(totalMove));

    vec2 nextAcc = totalMove - intStep;
    imageStore(prevAccTex, prevPx, vec4(nextAcc, 0, 0));

    ivec2 targetPx = currPx + ivec2(intStep);
    vec2 targetUV = (vec2(targetPx) + 0.5) / vec2(size);


    if (targetPx.x < 0 || targetPx.x >= size.x || targetPx.y < 0 || targetPx.y >= size.y) return;

    if (reproject) {
        float targetDepth = texture(currDepthTex, targetUV).r;
        if (targetDepth >= 1.0) return;

        float depthDiff = abs(currNDC.z - (targetDepth * 2.0 - 1.0));
        if (depthDiff > 0.01) return; // TODO: experiment with smaller values
    }


    imageStore(currNoiseTex, targetPx, vec4(prevNoise.r, 1, 0, 0));
    imageStore(currAccTex, targetPx, vec4(nextAcc, 0, 0));
}

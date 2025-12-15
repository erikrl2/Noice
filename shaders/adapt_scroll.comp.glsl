#version 430 core
layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba8, binding = 0) uniform readonly image2D prevNoiseTex;
layout(rgba8, binding = 1) uniform image2D currNoiseTex;
layout(rg16f, binding = 2) uniform readonly image2D prevAccTex;
layout(rg16f, binding = 3) uniform image2D currAccTex;

uniform sampler2D flowTex;
uniform float scrollSpeed;

void main() {
    ivec2 size = imageSize(prevNoiseTex);
    ivec2 prevPx = ivec2(gl_GlobalInvocationID.xy);
    if (prevPx.x >= size.x || prevPx.y >= size.y) return;

    // 1. Lese alten Zustand
    vec4 noise = imageLoad(prevNoiseTex, prevPx);
    if (noise.a < 0.5) return;

    // 2. Lese Flow (Tangent Map) an aktueller Position
    vec2 uv = (vec2(prevPx) + 0.5) / vec2(size);
    vec4 flowColor = texture(flowTex, uv);
    
    if (flowColor.a < 0.5) return;

    // Decode Flow: (0.5, 0.0) -> (0, -1)
#if 1
    vec2 dir = flowColor.rg * 2.0 - 1.0;
    if (abs(dir.x) < 0.01) dir.x = 0.0; // Snap to 0
    dir = normalize(dir);
#else
    vec2 dir = normalize(flowColor.rg * 2.0 - 1.0);
#endif
    vec2 flow = dir * scrollSpeed;

    // 3. Accumulate
    vec2 prevAcc = imageLoad(prevAccTex, prevPx).xy;
    vec2 totalMove = prevAcc + flow;
    
    vec2 intStep = trunc(totalMove);
    vec2 nextAcc = totalMove - intStep;

    // 4. Ziel-Position
    ivec2 targetPx = prevPx + ivec2(intStep);

    if (targetPx.x >= 0 && targetPx.x < size.x && targetPx.y >= 0 && targetPx.y < size.y) {
        imageStore(currNoiseTex, targetPx, noise);
        imageStore(currAccTex, targetPx, vec4(nextAcc, 0, 0));
    }
}
#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba8, binding = 0) uniform readonly image2D prevNoiseTex;
layout(rgba8, binding = 1) uniform image2D currNoiseTex;
layout(rg16f, binding = 2) uniform readonly image2D prevScrollAccTex;
layout(rg16f, binding = 3) uniform image2D currScrollAccTex;
layout(r32ui, binding = 4) uniform uimage2D lockTex;

uniform sampler2D prevDepthTex;
uniform sampler2D currDepthTex;
uniform sampler2D objectTex;
uniform sampler2D tangentTex;

uniform mat4 prevModel;
uniform mat4 currModel;
uniform mat4 invPrevModel;
uniform mat4 invCurrModel;
uniform mat4 invPrevView;
uniform mat4 invPrevProj;
uniform mat4 prevViewProj;
uniform mat4 currViewProj;

uniform vec2 fullResolution;
uniform int downscaleFactor;
uniform float normalScrollSpeed;
uniform float deltaTime;

void main() {
    ivec2 prevNoisePx = ivec2(gl_GlobalInvocationID.xy);
    ivec2 noiseSize = imageSize(prevNoiseTex);
    
    if (prevNoisePx.x >= noiseSize.x || prevNoisePx.y >= noiseSize.y) {
        return;
    }
    
    vec4 prevNoise = imageLoad(prevNoiseTex, prevNoisePx);
    
    vec2 prevFullPx = (vec2(prevNoisePx) + vec2(0.5)) * float(downscaleFactor);
    vec2 prevFullUV = prevFullPx / fullResolution;
    
    float prevDepth = texture(prevDepthTex, prevFullUV).r;
    
    if (prevDepth >= 1.0) {
        imageStore(currNoiseTex, prevNoisePx, vec4(prevNoise.rgb, 1.0));
        imageStore(currScrollAccTex, prevNoisePx, vec4(0.0));
        return;
    }
    
    // --- Reprojection ---
    vec3 prevNDC;
    prevNDC.xy = prevFullUV * 2.0 - 1.0;
    prevNDC.z = prevDepth * 2.0 - 1.0;
    
    vec4 prevClip = vec4(prevNDC, 1.0);
    vec4 prevViewPos = invPrevProj * prevClip;
    prevViewPos /= prevViewPos.w;
    vec4 prevWorldPos = invPrevView * vec4(prevViewPos.xyz, 1.0);
    
    vec4 localPos = invPrevModel * prevWorldPos;
    vec4 currWorldPos = currModel * localPos;
    vec4 currClip = currViewProj * currWorldPos;
    
    if (currClip.w <= 0.0) return;
    
    vec3 currNDC = currClip.xyz / currClip.w;
    
    if (abs(currNDC.x) > 1.0 || abs(currNDC.y) > 1.0 || abs(currNDC.z) > 1.0) {
        return;
    }
    
    vec2 currScreenPos = (currNDC.xy * 0.5 + 0.5) * fullResolution;
    
    // --- Flow & Accumulation ---
    vec3 tangentEncoded = texture(tangentTex, currScreenPos / fullResolution).rgb;
    vec2 tangent = normalize(tangentEncoded * 2.0 - 1.0).xy;
    
    vec2 prevAcc = imageLoad(prevScrollAccTex, prevNoisePx).xy;
    
    // Flow in Noise-Pixeln (korrigiert)
    vec2 flow = (tangent * normalScrollSpeed * deltaTime) / float(downscaleFactor);
    
    vec2 totalMove = prevAcc + flow;
    vec2 intStep = round(totalMove);
    vec2 nextAcc = totalMove - intStep;
    
    // Ziel-Position im Noise-Grid
    // Wir nehmen die reprojizierte Position im Noise-Grid und addieren den Step
    vec2 currNoisePosFloat = currScreenPos / float(downscaleFactor);
    ivec2 baseTargetPx = ivec2(floor(currNoisePosFloat));
    ivec2 currNoisePx = baseTargetPx + ivec2(intStep);
    
    // Check Bounds
    if (currNoisePx.x < 0 || currNoisePx.x >= noiseSize.x ||
        currNoisePx.y < 0 || currNoisePx.y >= noiseSize.y) {
        return;
    }
    
    // Check Depth/Object
    vec3 objColor = texture(objectTex, (vec2(currNoisePx) + 0.5) / vec2(noiseSize)).rgb;
    if (objColor != vec3(1.0, 0.0, 0.0)) return;
    
    vec2 currFullUV = (vec2(currNoisePx) * float(downscaleFactor) + 0.5 * float(downscaleFactor)) / fullResolution;
    float currDepth = texture(currDepthTex, currFullUV).r;
    if (currDepth >= 1.0) return;
    
    float depthDiff = abs(currNDC.z - (currDepth * 2.0 - 1.0));
    if (depthDiff > 0.01) return;
    
    // --- Atomic Lock & Write ---
    // Tiefe mappen auf uint (0 = nah, MAX = fern)
    // currNDC.z ist -1..1. Wir mappen auf 0..1
    float depth01 = currNDC.z * 0.5 + 0.5;
    uint depthUint = uint(depth01 * 4294967295.0);
    
    imageAtomicMin(lockTex, currNoisePx, depthUint);
    memoryBarrierImage();
    
    if (imageLoad(lockTex, currNoisePx).r == depthUint) {
        imageStore(currNoiseTex, currNoisePx, vec4(prevNoise.rgb, 1.0));
        imageStore(currScrollAccTex, currNoisePx, vec4(nextAcc, 0.0, 0.0));
    }
}
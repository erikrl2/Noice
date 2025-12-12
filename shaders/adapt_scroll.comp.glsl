#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba8, binding = 0) uniform readonly image2D prevNoiseTex;
layout(rgba8, binding = 1) uniform image2D currNoiseTex;

uniform sampler2D prevDepthTex;
uniform sampler2D currDepthTex;
uniform sampler2D objectTex;
uniform sampler2D prevNormalTex;

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

// TODO: find out if all those safety checks are nessessary

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
        return;
    }
    
    vec3 prevNormalEncoded = texture(prevNormalTex, prevFullUV).rgb;
    vec3 prevNormal = normalize(prevNormalEncoded * 2.0 - 1.0);
    
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
    
    vec3 localNormal = normalize(mat3(invPrevModel) * prevNormal);
    vec3 currWorldNormal = normalize(mat3(currModel) * localNormal);
    
    vec4 normalClip = currViewProj * vec4(currWorldNormal, 0.0);
    vec2 screenSpaceNormal = normalClip.xy;
    
    vec2 tangent = vec2(-screenSpaceNormal.y, screenSpaceNormal.x);
    float tangentLength = length(tangent);
    
    if (tangentLength > 0.0001) {
        tangent = tangent / tangentLength;
    } else {
        tangent = vec2(1.0, 0.0);
    }
    
    vec2 scrollOffset = tangent * normalScrollSpeed * float(downscaleFactor) * deltaTime;
    
    vec2 finalScreenPos = currScreenPos + scrollOffset;
    vec2 currFullUV = finalScreenPos / fullResolution;
    
    float currDepth = texture(currDepthTex, currFullUV).r;
    if (currDepth >= 1.0) return;
    
    float depthDiff = abs(currNDC.z - (currDepth * 2.0 - 1.0));
    if (depthDiff > 0.01) return;
    
    vec2 currNoisePxFloat = finalScreenPos / float(downscaleFactor);
    ivec2 currNoisePx = ivec2(currNoisePxFloat);
    
    if (currNoisePx.x < 0 || currNoisePx.x >= noiseSize.x ||
        currNoisePx.y < 0 || currNoisePx.y >= noiseSize.y) {
        return;
    }
    
    vec3 objColor = texture(objectTex, (vec2(currNoisePx) + 0.5) / vec2(noiseSize)).rgb;
    if (objColor != vec3(1.0, 0.0, 0.0)) return;
    
    vec4 existingValue = imageLoad(currNoiseTex, currNoisePx);
    if (existingValue.a < 0.5) {
        imageStore(currNoiseTex, currNoisePx, vec4(prevNoise.rgb, 1.0));
    }
}
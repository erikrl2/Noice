#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba8, binding = 0) uniform readonly image2D prevNoiseTex;
layout(rgba8, binding = 1) uniform writeonly image2D currNoiseTex;

uniform sampler2D prevDepthTex;
uniform sampler2D objectTex;
uniform mat4 prevViewProj;
uniform mat4 currViewProj;
uniform mat4 invPrevProj;
uniform mat4 invPrevView;
uniform vec2 fullResolution;
uniform int downscaleFactor;

void main() {
    ivec2 prevNoisePx = ivec2(gl_GlobalInvocationID.xy);
    ivec2 noiseSize = imageSize(prevNoiseTex);
    
    if (prevNoisePx.x >= noiseSize.x || prevNoisePx.y >= noiseSize.y) {
        return;
    }
    
    vec4 prevNoise = imageLoad(prevNoiseTex, prevNoisePx);
    #if 0
    imageStore(currNoiseTex, prevNoisePx, prevNoise);
    return;
    #endif
    
    vec2 prevFullPx = (vec2(prevNoisePx) + vec2(0.5)) * float(downscaleFactor);
    vec2 prevFullUV = prevFullPx / fullResolution;
    
    float prevDepth = texture(prevDepthTex, prevFullUV).r;
    
    if (prevDepth >= 1.0) {
        return;
    }
    
    vec3 prevNDC;
    prevNDC.xy = prevFullUV * 2.0 - 1.0;
    prevNDC.z = prevDepth * 2.0 - 1.0;
    
    vec4 prevClip = vec4(prevNDC, 1.0);
    vec4 prevViewPos = invPrevProj * prevClip;
    prevViewPos /= prevViewPos.w;
    vec4 worldPos = invPrevView * vec4(prevViewPos.xyz, 1.0);
    
    vec4 currClip = currViewProj * worldPos;
    if (currClip.w <= 0.0) return;
    
    vec3 currNDC = currClip.xyz / currClip.w;
    
    if (abs(currNDC.x) > 1.0 || abs(currNDC.y) > 1.0 || abs(currNDC.z) > 1.0) {
        return;
    }
    
    vec2 currScreen = (currNDC.xy * 0.5 + 0.5) * fullResolution;
    vec2 currNoisePxFloat = currScreen / float(downscaleFactor);
    
    // FIX: Benutze round() statt floor(x + 0.5)
    ivec2 currNoisePx = ivec2(round(currNoisePxFloat));
    
    if (currNoisePx.x < 0 || currNoisePx.x >= noiseSize.x ||
        currNoisePx.y < 0 || currNoisePx.y >= noiseSize.y) {
        return;
    }
    
    vec3 objColor = texture(objectTex, (vec2(currNoisePx) + 0.5) / vec2(noiseSize)).rgb;
    if (objColor != vec3(1.0, 0.0, 0.0)) {
        return;
    }
    
    imageStore(currNoiseTex, currNoisePx, prevNoise);
}

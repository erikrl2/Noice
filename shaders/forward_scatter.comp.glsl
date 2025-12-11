#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba8, binding = 0) uniform readonly image2D prevNoiseTex;
layout(rgba8, binding = 1) uniform image2D currNoiseTex;

uniform sampler2D prevDepthTex;
uniform sampler2D currDepthTex;
uniform sampler2D objectTex;
uniform sampler2D prevNormalTex;
uniform sampler2D currNormalTex;

uniform mat4 prevModel;
uniform mat4 currModel;
uniform mat4 invPrevModel;
uniform mat4 invCurrModel;
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
    
    vec2 prevFullPx = (vec2(prevNoisePx) + vec2(0.5)) * float(downscaleFactor);
    vec2 prevFullUV = prevFullPx / fullResolution;
    
    float prevDepth = texture(prevDepthTex, prevFullUV).r;
    
    // === HINTERGRUND: Behalte an gleicher Position ===
    if (prevDepth >= 1.0) {
        imageStore(currNoiseTex, prevNoisePx, vec4(prevNoise.rgb, 1.0));
        return;
    }
    
    // === Lade Previous Normal ===
    vec3 prevNormalEncoded = texture(prevNormalTex, prevFullUV).rgb;
    vec3 prevNormal = normalize(prevNormalEncoded * 2.0 - 1.0);
    
    // === Standard Forward Projection (wie bisher) ===
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
    
    // === NEU: Berechne Screen-Space Normal ===
    // Transformiere Normale durch die gleichen Transformationen
    vec3 localNormal = normalize(mat3(invPrevModel) * prevNormal);
    vec3 currWorldNormal = normalize(mat3(currModel) * localNormal);
    
    // Projiziere Normale zu Screen Space (ohne Translation)
    vec4 normalClip = currViewProj * vec4(currWorldNormal, 0.0);
    vec2 screenSpaceNormal = normalize(normalClip.xy);
    
    // === NEU: Berechne Motion Vector ===
    vec2 prevScreenPos = prevFullPx;
    vec2 currScreenPos = (currNDC.xy * 0.5 + 0.5) * fullResolution;
    vec2 motionVector = currScreenPos - prevScreenPos;
    
    // === NEU: Projiziere Motion auf Tangentialebene ===
    // Tangente = senkrecht zur projizierten Normale
    vec2 tangent = vec2(-screenSpaceNormal.y, screenSpaceNormal.x);
    
    // Berechne tangentiale Komponente der Bewegung
    float tangentialMotion = dot(motionVector, tangent);
    
    // Finale Position: Basispunkt + tangentiale Bewegung
    vec2 currScreenFullRes = currScreenPos + tangent * tangentialMotion * 0.5;  // 0.5 = Dämpfung
    vec2 currFullUV = currScreenFullRes / fullResolution;
    
    // === Rest wie bisher ===
    float currDepth = texture(currDepthTex, currFullUV).r;
    if (currDepth >= 1.0) return;
    
    float depthDiff = abs(currNDC.z - (currDepth * 2.0 - 1.0));
    if (depthDiff > 0.01) return;
    
    vec2 currNoisePxFloat = currScreenFullRes / float(downscaleFactor);
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
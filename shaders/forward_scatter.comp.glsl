#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba8, binding = 0) uniform readonly image2D prevNoiseTex;
layout(rgba8, binding = 1) uniform image2D currNoiseTex;

uniform sampler2D prevDepthTex;
uniform sampler2D currDepthTex;
uniform sampler2D objectTex;
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
    
    // Lade Noise vom vorherigen Frame
    vec4 prevNoise = imageLoad(prevNoiseTex, prevNoisePx);
    
    // Berechne Full-Resolution UV für diesen Noise-Pixel
    vec2 prevFullPx = (vec2(prevNoisePx) + vec2(0.5)) * float(downscaleFactor);
    vec2 prevFullUV = prevFullPx / fullResolution;
    
    // Sample Previous Depth
    float prevDepth = texture(prevDepthTex, prevFullUV).r;
    
    // === HINTERGRUND: Behalte an gleicher Position ===
    if (prevDepth >= 1.0) {
        // Hintergrund  Schreibe direkt an gleiche Position
        imageStore(currNoiseTex, prevNoisePx, vec4(prevNoise.rgb, 1.0));
        return;
    }
    
    // === OBJEKT: Forward Projection MIT Model-Matrix ===
    
    // Schritt 1: Rekonstruiere WORLD Position vom PREVIOUS Frame
    vec3 prevNDC;
    prevNDC.xy = prevFullUV * 2.0 - 1.0;
    prevNDC.z = prevDepth * 2.0 - 1.0;
    
    vec4 prevClip = vec4(prevNDC, 1.0);
    vec4 prevViewPos = invPrevProj * prevClip;
    prevViewPos /= prevViewPos.w;
    vec4 prevWorldPos = invPrevView * vec4(prevViewPos.xyz, 1.0);
    
    // Schritt 2: Transformiere zu LOCAL Space (des PREVIOUS Objekts)
    vec4 localPos = invPrevModel * prevWorldPos;
    
    // Schritt 3: Transformiere zu CURRENT World Space (mit CURRENT Model)
    vec4 currWorldPos = currModel * localPos;
    
    // Schritt 4: Projiziere zu CURRENT Screen Space
    vec4 currClip = currViewProj * currWorldPos;
    
    if (currClip.w <= 0.0) {
        // Hinter der Kamera im aktuellen Frame
        return;
    }
    
    vec3 currNDC = currClip.xyz / currClip.w;
    
    // Außerhalb des Bildschirms
    if (abs(currNDC.x) > 1.0 || abs(currNDC.y) > 1.0 || abs(currNDC.z) > 1.0) {
        return;
    }
    
    // Berechne Screen Position im aktuellen Frame
    vec2 currScreenFullRes = (currNDC.xy * 0.5 + 0.5) * fullResolution;
    vec2 currFullUV = currScreenFullRes / fullResolution;
    
    // Sample Current Depth am Zielpixel
    float currDepth = texture(currDepthTex, currFullUV).r;
    
    // Kein Objekt im aktuellen Frame  Nicht schreiben
    if (currDepth >= 1.0) {
        return;
    }
    
    // Depth-Vergleich: Ist das der gleiche Oberflächenpunkt?
    float depthDiff = abs(currNDC.z - (currDepth * 2.0 - 1.0));
    if (depthDiff > 0.01) {
        // Zu große Depth-Differenz  Wahrscheinlich Occlusion
        return;
    }
    
    // Konvertiere zu Noise-Koordinaten
    vec2 currNoisePxFloat = currScreenFullRes / float(downscaleFactor);
    ivec2 currNoisePx = ivec2(currNoisePxFloat);
    
    // Bounds check
    if (currNoisePx.x < 0 || currNoisePx.x >= noiseSize.x ||
        currNoisePx.y < 0 || currNoisePx.y >= noiseSize.y) {
        return;
    }
    
    // Prüfe ob Zielpixel auf Objekt liegt (rot = gefülltes Objekt)
    vec3 objColor = texture(objectTex, (vec2(currNoisePx) + 0.5) / vec2(noiseSize)).rgb;
    if (objColor != vec3(1.0, 0.0, 0.0)) {
        // Wireframe oder Hintergrund
        return;
    }
    
    // === ATOMIC WRITE: Verhindere Race Conditions ===
    // Schreibe nur, wenn Zielpixel noch leer ist (Alpha < 0.5)
    vec4 existingValue = imageLoad(currNoiseTex, currNoisePx);
    if (existingValue.a < 0.5) {
        // Setze Alpha = 1.0 um zu markieren, dass dieser Pixel geschrieben wurde
        imageStore(currNoiseTex, currNoisePx, vec4(prevNoise.rgb, 1.0));
    }
}
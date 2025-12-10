#version 430 core

out vec4 FragColor;

uniform sampler2D prevTex;
uniform sampler2D objectTex;
uniform sampler2D objectDepthTex;
uniform sampler2D prevDepthTex;

uniform float doXor;
uniform float rand;
uniform mat4 prevViewProj;
uniform mat4 invCurrProj;
uniform mat4 invCurrView;
uniform vec2 fullResolution;
uniform int downscaleFactor;

float rng() {
    ivec2 p = ivec2(gl_FragCoord.xy);
    uint x = uint(p.x);
    uint y = uint(p.y);
    uint seed = floatBitsToUint(rand);
    uint n = x * 1664525u + y * 1013904223u + seed * 1103515245u;
    n ^= (n >> 16);
    n *= 0x45d9f3bdu;
    n ^= (n >> 16);
    return float(n) * 2.3283064365386963e-10;
}

void main() {
    ivec2 noiseSize = textureSize(prevTex, 0);
    ivec2 fragPx = ivec2(gl_FragCoord.xy);

    vec2 fullPx = (vec2(fragPx) + vec2(0.5)) * float(downscaleFactor);
    vec2 fullUV = fullPx / fullResolution;

    vec3 objColor = texture(objectTex, (vec2(fragPx) + 0.5) / vec2(noiseSize)).rgb;

    // Wireframe: Zeige vorheriges Noise-Feld
    if (objColor != vec3(1.0, 0.0, 0.0)) {
        vec3 noiseColor = texelFetch(prevTex, fragPx, 0).rgb;
        FragColor = vec4(doXor == 1.0 ? abs(objColor - noiseColor) : noiseColor, 1.0);
        return;
    }

    // === ADAPTIVE SCROLL (Backward Mapping mit Depth Check) ===
    
    float currDepth = texture(objectDepthTex, fullUV).r;
    
    // Kein Objekt  Erzeuge neues Noise
    if (currDepth >= 1.0) {
        FragColor = vec4(vec3(step(0.5, rng())), 1.0);
        return;
    }
    
    // Rekonstruiere World Position (CURRENT frame)
    vec3 currNDC;
    currNDC.xy = fullUV * 2.0 - 1.0;
    currNDC.z = currDepth * 2.0 - 1.0;
    
    vec4 currClip = vec4(currNDC, 1.0);
    vec4 currViewPos = invCurrProj * currClip;
    currViewPos /= currViewPos.w;
    vec4 worldPos = invCurrView * vec4(currViewPos.xyz, 1.0);
    
    // Projiziere zu PREVIOUS frame
    vec4 prevClip = prevViewProj * worldPos;
    
    // Hinter der Kamera im vorherigen Frame  Neues Noise
    if (prevClip.w <= 0.0) {
        FragColor = vec4(vec3(step(0.5, rng())), 1.0);
        return;
    }
    
    vec3 prevNDC = prevClip.xyz / prevClip.w;
    
    // Außerhalb des Bildschirms im vorherigen Frame  Neues Noise
    if (abs(prevNDC.x) > 1.0 || abs(prevNDC.y) > 1.0 || abs(prevNDC.z) > 1.0) {
        FragColor = vec4(vec3(step(0.5, rng())), 1.0);
        return;
    }
    
    // Berechne Previous Screen Position in FULL-RES
    vec2 prevScreenFullRes = (prevNDC.xy * 0.5 + 0.5) * fullResolution;
    vec2 prevFullUV = prevScreenFullRes / fullResolution;
    
    // Prüfe Previous Depth: War dort überhaupt ein Objekt?
    float prevDepth = texture(prevDepthTex, prevFullUV).r;
    
    if (prevDepth >= 1.0) {
        // Im vorherigen Frame war hier kein Objekt  Neues Noise
        FragColor = vec4(vec3(step(0.5, rng())), 1.0);
        return;
    }
    
    // Depth-Vergleich (Occlusion Check)
    float depthDiff = abs(prevNDC.z - (prevDepth * 2.0 - 1.0));
    if (depthDiff > 0.01) {  // REDUZIERT: Toleranz von 0.05 auf 0.01
        FragColor = vec4(vec3(step(0.5, rng())), 1.0);
        return;
    }
    
    // Konvertiere zu Noise-Koordinaten - FIX: Korrekte Rundung!
    vec2 prevNoisePxFloat = prevScreenFullRes / float(downscaleFactor);
    ivec2 prevNoisePx = ivec2(prevNoisePxFloat);  // Einfaches Truncate statt floor+0.5
    
    // Bounds check
    if (prevNoisePx.x < 0 || prevNoisePx.x >= noiseSize.x ||
        prevNoisePx.y < 0 || prevNoisePx.y >= noiseSize.y) {
        FragColor = vec4(vec3(step(0.5, rng())), 1.0);
        return;
    }
    
    // Sample vom Previous Frame (Noise bleibt "haften")
    FragColor = vec4(texelFetch(prevTex, prevNoisePx, 0).rgb, 1.0);
}
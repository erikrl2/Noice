#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba8, binding = 0) uniform image2D noiseTex;
layout(rg16f, binding = 1) uniform image2D currScrollAccTex;

uniform float rand;

uint hash(uint x) {
    x ^= x >> 16;
    x *= 0x45d9f3bdu;
    x ^= x >> 16;
    return x;
}

float rng(ivec2 px, float seed) {
    uint x = uint(px.x);
    uint y = uint(px.y);
    uint s = floatBitsToUint(seed);
    uint n = x * 1664525u + y * 1013904223u + s * 1103515245u;
    n = hash(n);
    return float(n) * 2.3283064365386963e-10;
}

void main() {
    ivec2 px = ivec2(gl_GlobalInvocationID.xy);
    ivec2 noiseSize = imageSize(noiseTex);
    
    if (px.x >= noiseSize.x || px.y >= noiseSize.y) {
        return;
    }
    
    // Wenn das Pixel leer ist (Gap), füllen wir es
    if (imageLoad(noiseTex, px).a < 0.5) {
        // 1. Noise generieren
        vec3 newNoise = vec3(step(0.5, rng(px, rand)));
        imageStore(noiseTex, px, vec4(newNoise, 1.0));
        
        // 2. Accumulator von Nachbarn erben (Phase Sync)
        vec2 estimatedAcc = vec2(0.0);
        float weight = 0.0;
        
        // Wir prüfen die 4 direkten Nachbarn
        ivec2 offsets[4] = {ivec2(1,0), ivec2(-1,0), ivec2(0,1), ivec2(0,-1)};
        
        for(int i = 0; i < 4; ++i) {
            ivec2 nPos = px + offsets[i];
            
            // Bounds Check
            if(nPos.x >= 0 && nPos.x < noiseSize.x && nPos.y >= 0 && nPos.y < noiseSize.y) {
                // Prüfen ob Nachbar valide ist (bereits existierendes Noise)
                // Wir lesen noiseTex. Da wir selbst gerade erst schreiben, lesen wir hier
                // entweder alte Werte oder Werte von anderen Threads.
                // Da wir nur schreiben wenn a < 0.5, bedeutet a > 0.5, dass der Nachbar
                // ein "echtes" Pixel aus dem adapt_scroll Pass ist.

                //vec4 nNoise = imageLoad(noiseTex, nPos);
                //if(nNoise.a > 0.5) {
                vec2 nAcc = imageLoad(currScrollAccTex, nPos).xy;
                if(nAcc.rg != vec2(0)) {
                    //vec2 nAcc = imageLoad(currScrollAccTex, nPos).xy;
                    estimatedAcc += nAcc;
                    weight += 1.0;
                }
            }
        }
        
        if(weight > 0.0) {
            estimatedAcc /= weight;
        }
        
        // Speichere den geschätzten Accumulator, damit das Pixel im Takt bleibt
        imageStore(currScrollAccTex, px, vec4(estimatedAcc, 0.0, 0.0));
    }
}
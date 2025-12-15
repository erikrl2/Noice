#version 430 core
layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba8, binding = 0) uniform image2D currNoiseTex;
layout(rg16f, binding = 1) uniform image2D currAccTex;

uniform float rand;

// Simple Hash
float rng(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    ivec2 px = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(currNoiseTex);
    if (px.x >= size.x || px.y >= size.y) return;

    // Prüfen ob Pixel leer ist
    vec4 val = imageLoad(currNoiseTex, px);
    if (val.a < 0.1) {
        // Ist es auf dem Dreieck? (Wir könnten flowTex lesen, aber hier füllen wir einfach alles)
        // Für den Test okay.
        
        float r = rng(vec2(px) + rand);
        float noiseVal = step(0.5, r);
        
        imageStore(currNoiseTex, px, vec4(vec3(noiseVal), 1.0));
        // Acc auf 0 setzen (oder Nachbarn, aber erst mal 0 für Test)
        imageStore(currAccTex, px, vec4(0.0));
    }
}
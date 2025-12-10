#version 430 core

out vec4 FragColor;

uniform sampler2D prevTex;        // noise (downscaled)
uniform sampler2D objectTex;      // object color (downsized sampling)
uniform sampler2D objectDepthTex; // full-res depth of objectFB

uniform float doXor;
uniform float rand;
uniform mat4 prevViewProj;        // previous frame view*proj
uniform mat4 invProj;             // inverse of current projection matrix
uniform mat4 invView;             // inverse of current view matrix
uniform vec2 fullResolution;      // full framebuffer size (width, height)
uniform int downscaleFactor;      // how many screen pixels per noise texel

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
    // prevTex is downscaled noise texture
    ivec2 noiseSize = textureSize(prevTex, 0);
    ivec2 fragPx = ivec2(gl_FragCoord.xy); // coordinates in noise texture space

    // Map noise fragment -> full resolution pixel center (for depth sampling & screen coords)
    // Use +0.5 to sample pixel center in full-res
    vec2 fullPx = (vec2(fragPx) + vec2(0.5)) * float(downscaleFactor);
    vec2 fullUV = fullPx / fullResolution; // [0,1] in full-res space

    // sample object color at noise resolution (as before)
    vec3 objColor = texture(objectTex, (vec2(fragPx) + 0.5) / vec2(noiseSize)).rgb;

    if (objColor != vec3(1.0, 0.0, 0.0)) { // xor (flickers wireframe)
        vec3 noiseColor = texelFetch(prevTex, fragPx, 0).rgb;
        FragColor = vec4(doXor == 1.0 ? abs(objColor - noiseColor) : noiseColor, 1.0);
        return;
    }

    // --- scroll faces: compute per-pixel motion from depth + prevViewProj
    float depth = texture(objectDepthTex, fullUV).r;

    // reconstruct NDC for current frame
    vec3 ndc;
    ndc.xy = fullUV * 2.0 - 1.0;       // x,y in NDC
    ndc.z = depth * 2.0 - 1.0;         // z in NDC (OpenGL depth -> NDC)

    // reconstruct view-space position, then world position
    vec4 clip = vec4(ndc, 1.0);
    vec4 viewPos = invProj * clip;
    viewPos /= viewPos.w;
    vec4 worldPos = invView * vec4(viewPos.xyz, 1.0);

    // project to previous clip space
    vec4 prevClip = prevViewProj * worldPos;
    vec3 prevNDC = prevClip.xyz / prevClip.w;

    // current screen pos in full-res pixels
    vec2 currScreen = (ndc.xy * 0.5 + 0.5) * fullResolution;
    vec2 prevScreen = (prevNDC.xy * 0.5 + 0.5) * fullResolution;

    // motion in screen pixels (prev - curr)
    vec2 motionScreen = prevScreen - currScreen;

    // convert to noise pixels
    vec2 motionNoise = motionScreen / float(downscaleFactor);

    // combine with global scroll offset (already in noise pixels)
    vec2 totalOffset = motionNoise;

    // sample the object color at the offset position to decide if face exists at next pos
    ivec2 nextPx = fragPx + ivec2(floor(totalOffset + 0.5));
    ivec2 wrappedPx = ivec2(
        ((nextPx.x % noiseSize.x) + noiseSize.x) % noiseSize.x,
        ((nextPx.y % noiseSize.y) + noiseSize.y) % noiseSize.y
    );

    vec3 objColorAtNext = texture(objectTex, (vec2(nextPx) + 0.5) / vec2(noiseSize)).rgb;
    if (objColorAtNext != vec3(1.0, 0.0, 0.0)) {
        FragColor = vec4(vec3(step(0.5, rng())), 1.0);
    } else {
        FragColor = vec4(texelFetch(prevTex, wrappedPx, 0).rgb, 1.0);
    }
}

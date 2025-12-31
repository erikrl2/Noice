#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rg8, binding = 0) uniform image2D uCurrNoiseTex;
layout(rg8, binding = 1) uniform writeonly image2D uPrevNoiseTex;
layout(rg16f, binding = 2) uniform writeonly image2D uCurrAccTex;
layout(rg16f, binding = 3) uniform image2D uPrevAccTex;

uniform uint uSeed;

uint hash(uvec2 p) {
  uint h = p.x * 374761393u + p.y * 668265263u + uSeed;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h;
}

float rng(uvec2 p) {
  return float(hash(p)) * (1.0 / 4294967296.0);
}

void main() {
  ivec2 px = ivec2(gl_GlobalInvocationID.xy);
  ivec2 size = imageSize(uCurrNoiseTex);
  if (px.x >= size.x || px.y >= size.y) return;

  vec2 noise = imageLoad(uCurrNoiseTex, px).rg;
  if (noise.g < 0.5) {
    float noiseVal = step(0.5, rng(uvec2(px)));
    imageStore(uCurrNoiseTex, px, vec4(noiseVal, 1, 0, 0));

    vec2 inheritedAcc = imageLoad(uPrevAccTex, px).xy;
    imageStore(uCurrAccTex, px, vec4(inheritedAcc, 0, 0));
  }

  imageStore(uPrevNoiseTex, px, vec4(0));
  imageStore(uPrevAccTex, px, vec4(0));
}

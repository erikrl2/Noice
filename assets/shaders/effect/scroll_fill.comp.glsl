#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rg8, binding = 0) uniform image2D uCurrNoiseTex;
layout(rg8, binding = 1) uniform writeonly image2D uPrevNoiseTex;
layout(rg32f, binding = 2) uniform image2D uCurrAccTex;
layout(rg32f, binding = 3) uniform image2D uPrevAccTex;

layout(rgba16i, binding = 4) uniform readonly iimage2D uSeedMap;

layout(binding = 0) uniform isampler2D uCurrIdTex;
layout(binding = 1) uniform isampler2D uPrevIdTex;

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
    int currId = texelFetch(uCurrIdTex, px, 0).r;

    float noiseVal = step(0.5, rng(uvec2(px)));
    imageStore(uCurrNoiseTex, px, vec4(noiseVal, 1, 0, 0));

    vec2 acc = vec2(0);

    if (currId >= 0) {
      int prevId = texelFetch(uPrevIdTex, px, 0).r;

      if (prevId == currId) { // scroll holes
        acc = imageLoad(uPrevAccTex, px).xy; // use inherited acc
      } else { // disocclusion holes
        ivec4 seed = imageLoad(uSeedMap, px);
        if (seed.w != 0) {
          acc = imageLoad(uCurrAccTex, seed.xy).xy; // use nearest valid acc via JFA seed map
        }
      }
    }

    imageStore(uCurrAccTex, px, vec4(acc, 0, 0));
  }

  imageStore(uPrevNoiseTex, px, vec4(0));
  imageStore(uPrevAccTex, px, vec4(0));
}

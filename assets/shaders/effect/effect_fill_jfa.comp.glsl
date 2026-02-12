#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rg32f, binding = 2) uniform image2D uCurrAccTex;
layout(rgba16i, binding = 4) uniform readonly iimage2D uSeedMap;
layout(r8ui, binding = 6) uniform readonly uimage2D uNeedsJfaMask;

layout(binding = 0) uniform isampler2D uCurrIdTex;

void main() {
  ivec2 px = ivec2(gl_GlobalInvocationID.xy);
  ivec2 size = imageSize(uCurrAccTex);
  if (px.x >= size.x || px.y >= size.y) return;

  if (imageLoad(uNeedsJfaMask, px).r == 0u) return;

  int currId = texelFetch(uCurrIdTex, px, 0).r;
  if (currId < 0) return;

  ivec4 seed = imageLoad(uSeedMap, px);
  if (seed.w == 0) return;
  if (seed.z != currId) return;

  vec2 acc = imageLoad(uCurrAccTex, seed.xy).xy;
  imageStore(uCurrAccTex, px, vec4(acc, 0, 0));
}

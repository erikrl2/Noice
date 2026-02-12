#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba16i, binding = 0) uniform writeonly iimage2D uSeedOut; // seed = (x, y, id, valid)
layout(rg8, binding = 1) uniform readonly image2D uCurrNoiseTex;

layout(binding = 0) uniform isampler2D uCurrIdTex;

void main() {
  ivec2 px = ivec2(gl_GlobalInvocationID.xy);
  ivec2 size = imageSize(uCurrNoiseTex);
  if (px.x >= size.x || px.y >= size.y) return;

  vec2 noise = imageLoad(uCurrNoiseTex, px).rg;
  int id = texelFetch(uCurrIdTex, px, 0).r;

  if (id >= 0 && noise.g > 0.5) {
    imageStore(uSeedOut, px, ivec4(px.x, px.y, id, 1));
  } else {
    imageStore(uSeedOut, px, ivec4(0, 0, -1, 0));
  }
}

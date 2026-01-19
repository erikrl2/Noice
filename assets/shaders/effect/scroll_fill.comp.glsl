#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rg8, binding = 0) uniform image2D uCurrNoiseTex;
layout(rg8, binding = 1) uniform writeonly image2D uPrevNoiseTex;
layout(rg32f, binding = 2) uniform image2D uCurrAccTex;
layout(rg32f, binding = 3) uniform image2D uPrevAccTex;

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

bool findNeighborAccSpiral(ivec2 px, int id, ivec2 size, int maxR, out vec2 accOut) {
  for (int r = 1; r <= maxR; r++) {
    // top edge: (x from -r..r, y=-r)
    for (int dx = -r; dx <= r; dx++) {
      ivec2 q = px + ivec2(dx, -r);
      if (q.x < 0 || q.y < 0 || q.x >= size.x || q.y >= size.y) continue;
      if (texelFetch(uCurrIdTex, q, 0).r != id) continue;
      if (imageLoad(uCurrNoiseTex, q).g < 0.5) continue;
      accOut = imageLoad(uCurrAccTex, q).xy;
      return true;
    }

    // right edge: (x=+r, y from -r+1..r-1)
    for (int dy = -r + 1; dy <= r - 1; dy++) {
      ivec2 q = px + ivec2(r, dy);
      if (q.x < 0 || q.y < 0 || q.x >= size.x || q.y >= size.y) continue;
      if (texelFetch(uCurrIdTex, q, 0).r != id) continue;
      if (imageLoad(uCurrNoiseTex, q).g < 0.5) continue;
      accOut = imageLoad(uCurrAccTex, q).xy;
      return true;
    }

    // bottom edge: (x from r..-r, y=+r)
    for (int dx = r; dx >= -r; dx--) {
      ivec2 q = px + ivec2(dx, r);
      if (q.x < 0 || q.y < 0 || q.x >= size.x || q.y >= size.y) continue;
      if (texelFetch(uCurrIdTex, q, 0).r != id) continue;
      if (imageLoad(uCurrNoiseTex, q).g < 0.5) continue;
      accOut = imageLoad(uCurrAccTex, q).xy;
      return true;
    }

    // left edge: (x=-r, y from r-1..-r+1)
    for (int dy = r - 1; dy >= -r + 1; dy--) {
      ivec2 q = px + ivec2(-r, dy);
      if (q.x < 0 || q.y < 0 || q.x >= size.x || q.y >= size.y) continue;
      if (texelFetch(uCurrIdTex, q, 0).r != id) continue;
      if (imageLoad(uCurrNoiseTex, q).g < 0.5) continue;
      accOut = imageLoad(uCurrAccTex, q).xy;
      return true;
    }
  }
  return false;
}

void main() {
  ivec2 px = ivec2(gl_GlobalInvocationID.xy);
  ivec2 size = imageSize(uCurrNoiseTex);
  if (px.x >= size.x || px.y >= size.y) return;

  vec2 noise = imageLoad(uCurrNoiseTex, px).rg;
  if (noise.g < 0.5) {
    int currId = texelFetch(uCurrIdTex, px, 0).r;

    float noiseVal = step(0.5, rng(uvec2(px)));
    imageStore(uCurrNoiseTex, px, vec4(noiseVal, 0.25, 0, 0));

    vec2 acc = vec2(0);

    if (currId >= 0) {
      int prevId = texelFetch(uPrevIdTex, px, 0).r;

      if (prevId == currId) {
        acc = imageLoad(uPrevAccTex, px).xy; // inherited acc
      } else {
        vec2 neighAcc;
        const int MAX_R = 32;
        if (findNeighborAccSpiral(px, currId, size, MAX_R, neighAcc)) {
          acc = neighAcc;
        } else {
          acc = imageLoad(uPrevAccTex, px).xy;
        }
      }
    }

    imageStore(uCurrAccTex, px, vec4(acc, 0, 0));
  }

  imageStore(uPrevNoiseTex, px, vec4(0));
  imageStore(uPrevAccTex, px, vec4(0));
}

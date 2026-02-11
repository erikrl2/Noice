#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba16i, binding = 0) uniform readonly  iimage2D uSeedIn;
layout(rgba16i, binding = 1) uniform writeonly iimage2D uSeedOut;
layout(r32ui, binding = 5) uniform readonly uimage2D uNeedsJfaFlag;

layout(binding = 0) uniform isampler2D uCurrIdTex;

uniform int uStep;

void main() {
  if (imageLoad(uNeedsJfaFlag, ivec2(0, 0)).r == 0u) return;

  ivec2 px = ivec2(gl_GlobalInvocationID.xy);
  ivec2 size = imageSize(uSeedIn);
  if (px.x >= size.x || px.y >= size.y) return;

  int myId = texelFetch(uCurrIdTex, px, 0).r;
  if (myId < 0) {
    imageStore(uSeedOut, px, ivec4(0, 0, -1, 0));
    return;
  }

  ivec4 best = imageLoad(uSeedIn, px);
  float bestD2 = 3.4e38;

  if (best.w != 0 && best.z == myId) {
    ivec2 dI = best.xy - px;
    bestD2 = dot(dI, dI);
  } else {
    best = ivec4(0, 0, -1, 0);
  }

  bool zeroDistance = (bestD2 == 0.0);
  if (!zeroDistance) {
    for (int oy = -1; oy <= 1; oy++) {
      for (int ox = -1; ox <= 1; ox++) {
        if (ox == 0 && oy == 0) continue;

        ivec2 q = px + ivec2(ox * uStep, oy * uStep);
        if (q.x < 0 || q.y < 0 || q.x >= size.x || q.y >= size.y) continue;

        ivec4 cand = imageLoad(uSeedIn, q);
        if (cand.w == 0) continue;
        if (cand.z != myId) continue;

        ivec2 dI = cand.xy - px;
        float d2 = dot(dI, dI);
        if (d2 < bestD2) {
          bestD2 = d2;
          best = cand;
        }
      }
    }
  }

  imageStore(uSeedOut, px, best);
}

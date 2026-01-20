#version 430 core
layout(local_size_x = 16, local_size_y = 16) in;

layout(rg8,   binding = 0) uniform writeonly image2D uCurrNoiseTex;
layout(rg8,   binding = 1) uniform readonly  image2D uPrevNoiseTex;

layout(r32ui, binding = 2) uniform uimage2D uClaimTex;       // noiseRes

layout(rg16i, binding = 3) uniform readonly iimage2D uCurrStepTex; // noiseRes

layout(binding = 0) uniform isampler2D uPrevIdTex;     // full-res
layout(binding = 1) uniform isampler2D uCurrIdTex;     // full-res
layout(binding = 2) uniform sampler2D  uMotionPrevTex; // prev-space: currUV - prevUV (UV units)

uniform uint uFrameSalt; // changes every frame to decorrelate deterministic winner patterns (optional)

ivec2 pxFromUv(vec2 uv, ivec2 res) {
  ivec2 px = ivec2(round(uv * vec2(res) - 0.5));
  return clamp(px, ivec2(0), res - ivec2(1));
}

uint hash(uvec2 p) {
  uint h = p.x * 374761393u + p.y * 668265263u + uFrameSalt;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h;
}

void main() {
  ivec2 noiseRes = imageSize(uPrevNoiseTex);
  ivec2 prevPx = ivec2(gl_GlobalInvocationID.xy);
  if (prevPx.x >= noiseRes.x || prevPx.y >= noiseRes.y) return;

  vec2 prevNoise = imageLoad(uPrevNoiseTex, prevPx).rg;
  if (prevNoise.g < 0.1) return;

  ivec2 fullRes = textureSize(uMotionPrevTex, 0);
  vec2 prevUV = (vec2(prevPx) + 0.5) / vec2(noiseRes);
  ivec2 prevFullPx = ivec2(prevUV * vec2(fullRes));
  prevFullPx = clamp(prevFullPx, ivec2(0), fullRes - ivec2(1));

  int prevId = texelFetch(uPrevIdTex, prevFullPx, 0).r;
  if (prevId < 0) {
    if (texelFetch(uCurrIdTex, prevFullPx, 0).r < 0) {
      imageStore(uCurrNoiseTex, prevPx, vec4(prevNoise.r, 1, 0, 0));
      return;
    }
  }

  vec2 motion = texelFetch(uMotionPrevTex, prevFullPx, 0).xy; // currUV - prevUV
  vec2 currUV = prevUV + motion;

  // base target in noise pixels
  ivec2 baseTargetPx = pxFromUv(currUV, noiseRes);

  // validate target is still same object
  ivec2 baseTargetFullPx = ivec2(((vec2(baseTargetPx)+0.5)/vec2(noiseRes)) * vec2(fullRes));
  baseTargetFullPx = clamp(baseTargetFullPx, ivec2(0), fullRes - ivec2(1));
  int targetId = texelFetch(uCurrIdTex, baseTargetFullPx, 0).r;
  if (targetId != prevId) return;

  // add integer flow step computed from acc_update at that curr pixel
  ivec2 extraStep = imageLoad(uCurrStepTex, baseTargetPx).xy;
  ivec2 targetPx = baseTargetPx + extraStep;

  if (targetPx.x < 0 || targetPx.y < 0 || targetPx.x >= noiseRes.x || targetPx.y >= noiseRes.y) return;

  // Claim: use a randomized priority by hashing source; lower hash wins (optional).
  // Simplest: first thread wins is order-dependent; instead store hash and do atomicMin.
  uint myKey = hash(uvec2(prevPx));
  uint oldKey = imageAtomicMin(uClaimTex, targetPx, myKey);

  // Winner writes
  if (myKey <= oldKey) {
    imageStore(uCurrNoiseTex, targetPx, vec4(prevNoise.r, 1, 0, 0));
  }
}

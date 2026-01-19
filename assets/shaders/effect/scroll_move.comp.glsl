#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rg8, binding = 0) uniform writeonly image2D uCurrNoiseTex;
layout(rg8, binding = 1) uniform readonly image2D uPrevNoiseTex;
layout(rg32f, binding = 2) uniform writeonly image2D uCurrAccTex;
layout(rg32f, binding = 3) uniform image2D uPrevAccTex;
layout(r32ui, binding = 4) uniform uimage2D uClaimTex;

layout(binding = 0) uniform isampler2D uCurrIdTex;
layout(binding = 1) uniform isampler2D uPrevIdTex;
layout(binding = 2) uniform sampler2D uPrevLocalPosTex;
layout(binding = 3) uniform sampler2D uPrevFlowTex;

layout(std430, binding = 0) readonly buffer ObjectTransforms {
    mat4 modelMats[][2];
} b;

uniform mat4 uViewProj[2];

uniform float uScrollSpeed;
uniform bool uReproject;
uniform int uCurrInd;

// TODO: quantisize to angles
vec2 quantizePx(vec2 v, float q) {
  return round(v * q) / q;
}

vec2 uvFromWorld(vec3 worldPos, int i) {
  vec4 clip = uViewProj[i] * vec4(worldPos, 1);
  if (clip.w <= 0.0) return vec2(-1);

  vec3 ndc = clip.xyz / clip.w;
  //if (abs(ndc.x) > 1.0 || abs(ndc.y) > 1.0 || abs(ndc.z) > 1.0) return vec2(-1);

  return ndc.xy * 0.5 + 0.5;
}

vec2 uvFromLocal(vec3 localPos, int objID, int i) {
  vec3 worldPos = (b.modelMats[objID][i] * vec4(localPos, 1)).xyz;
  return uvFromWorld(worldPos, i);
}

vec2 flowPixelsPerWorldUnitFromLocal(vec3 localPos, vec3 localDir, int objID, vec2 viewportPx) {
  int prevInd = 1 - uCurrInd;

  vec3 worldPos = (b.modelMats[objID][prevInd] * vec4(localPos, 1)).xyz;
  vec3 worldDir = (b.modelMats[objID][prevInd] * vec4(localDir, 0)).xyz;

  float dirLen = length(worldDir);
  if (dirLen < 1e-20) return vec2(0);
  worldDir /= dirLen;

  float eps = 1.0;

  vec2 uv0 = uvFromWorld(worldPos, prevInd);

  vec2 uv1;
  uv1 = uvFromWorld(worldPos + worldDir * eps, prevInd);

  for (int k = 0; k < 8; k++) {
    uv1 = uvFromWorld(worldPos + worldDir * eps, prevInd);
    if (uv1.x > 0) break;
    eps *= 0.5;
  }

  vec2 dPx = (uv1 - uv0) * viewportPx;

  return dPx / eps;
}

void main() {
  ivec2 fullRes  = textureSize(uPrevFlowTex, 0);
  ivec2 noiseRes = imageSize(uPrevNoiseTex);

  ivec2 prevPx = ivec2(gl_GlobalInvocationID.xy);
  if (prevPx.x >= noiseRes.x || prevPx.y >= noiseRes.y) return;

  vec2 prevNoise = imageLoad(uPrevNoiseTex, prevPx).rg;
  if (prevNoise.g < 0.1) return; // not yet initialized

  vec2 prevUV = (vec2(prevPx) + 0.5) / vec2(noiseRes);
  ivec2 prevFullPx = ivec2(round(prevUV * vec2(fullRes) - 0.5));

  int prevId = texelFetch(uPrevIdTex, prevFullPx, 0).r;
  if (prevId < 0) {
    if (texelFetch(uCurrIdTex, prevFullPx, 0).r < 0) {
      imageStore(uCurrNoiseTex, prevPx, vec4(prevNoise.r, 1, 0, 0));
    }
    return;
  }

  vec2 reprojDelta = vec2(0);
  vec3 flowLocal = texelFetch(uPrevFlowTex, prevFullPx, 0).xyz;
  vec2 flowDir = flowLocal.xy;

  if (uReproject) {
    vec3 localPos = texelFetch(uPrevLocalPosTex, prevFullPx, 0).xyz;

    vec2 currUV = uvFromLocal(localPos, prevId, uCurrInd);
    vec2 prevUV = uvFromLocal(localPos, prevId, 1 - uCurrInd);

    //if (currUV.x < 0.0 || prevUV.x < 0.0) return;

    reprojDelta = (currUV - prevUV) * vec2(noiseRes);
    //reprojDelta = trunc(reprojDelta);

    flowDir = flowPixelsPerWorldUnitFromLocal(localPos, flowLocal, prevId, vec2(noiseRes));
  }

  vec2 prevAcc = imageLoad(uPrevAccTex, prevPx).xy;

  vec2 flow = flowDir * uScrollSpeed;

  flow = quantizePx(flow, 32.0);
  reprojDelta = quantizePx(reprojDelta, 128.0);

  vec2 totalMove = prevAcc + reprojDelta + flow;
  //totalMove = quantizePx(totalMove, 128.0);

  vec2 intStep = trunc(totalMove);
  vec2 nextAcc = totalMove - intStep;

  ivec2 targetPx = prevPx + ivec2(intStep);
  vec2 targetUV = (vec2(targetPx) + 0.5) / vec2(noiseRes);
  ivec2 targetFullPx = ivec2(round(targetUV * vec2(fullRes) - 0.5));

  if (targetPx.x < 0 || targetPx.x >= noiseRes.x || targetPx.y < 0 || targetPx.y >= noiseRes.y) return;

  int targetId = texelFetch(uCurrIdTex, targetFullPx, 0).r;
  if (targetId != prevId) return;

  //int asdf = texelFetch(uPrevIdTex, targetFullPx, 0).r;
  //if (asdf != prevId) {}

  //uint myKey = hash(uvec2(prevPx));
  uint myKey = uint(prevPx.x) + (uint(prevPx.y) << 16);
  uint oldKey = imageAtomicMin(uClaimTex, targetPx, myKey);
  if (myKey <= oldKey) {
    imageStore(uCurrNoiseTex, targetPx, vec4(prevNoise.r, 1, 0, 0));
    imageStore(uCurrAccTex, targetPx, vec4(nextAcc, 0, 0));
    imageStore(uPrevAccTex, prevPx, vec4(nextAcc, 0, 0));
  }
}

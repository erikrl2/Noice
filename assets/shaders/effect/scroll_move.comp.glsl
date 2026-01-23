#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rg8, binding = 0) uniform writeonly image2D uCurrNoiseTex;
layout(rg8, binding = 1) uniform readonly image2D uPrevNoiseTex;
layout(rg32f, binding = 2) uniform writeonly image2D uCurrAccTex;
layout(rg32f, binding = 3) uniform image2D uPrevAccTex;
layout(r32ui, binding = 4) uniform uimage2D uClaimTex;

layout(binding = 0) uniform isampler2D uCurrIdTex;
layout(binding = 1) uniform isampler2D uPrevIdTex;
layout(binding = 2) uniform sampler2D uPrevDepthTex;
layout(binding = 3) uniform sampler2D uPrevFlowTex;

layout(std430, binding = 0) readonly buffer ObjectTransforms {
    mat4 modelMats[][2];
} b;

uniform bool uReproject;
uniform mat4 uViewMat[2];
uniform mat4 uProjMat[2];
uniform int uCurrInd;

uniform bool uFlow;
uniform float uScrollSpeed;

vec2 quantizePx(vec2 v, float q) {
  return round(v * q) / q;
}

vec2 uvFromWorld(vec3 worldPos, int i) {
  vec4 clip = uProjMat[i] * uViewMat[i] * vec4(worldPos, 1);
  if (clip.w <= 0.0) return vec2(-1);
  vec3 ndc = clip.xyz / clip.w;
  return ndc.xy * 0.5 + 0.5;
}

vec2 uvFromLocal(vec3 localPos, int objID, int i) {
  vec3 worldPos = (b.modelMats[objID][i] * vec4(localPos, 1)).xyz;
  return uvFromWorld(worldPos, i);
}

void main() {
  ivec2 fullRes  = textureSize(uCurrIdTex, 0);
  ivec2 noiseRes = imageSize(uPrevNoiseTex);

  ivec2 prevPx = ivec2(gl_GlobalInvocationID.xy);
  if (prevPx.x >= noiseRes.x || prevPx.y >= noiseRes.y) return;

  vec2 prevNoise = imageLoad(uPrevNoiseTex, prevPx).rg;
  if (prevNoise.g < 0.1) return; // first frame only

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
  if (uReproject) {
    float prevDepth = texelFetch(uPrevDepthTex, prevFullPx, 0).x;
    int prevInd = 1 - uCurrInd;

    vec4 prevClipPos = vec4(vec3(prevUV, prevDepth) * 2.0 - 1.0, 1);
    vec4 prevViewPos = inverse(uProjMat[prevInd]) * prevClipPos;
    vec4 prevWorldPos = inverse(uViewMat[prevInd]) * vec4(prevViewPos.xyz / prevViewPos.w, 1);
    vec4 localPos = inverse(b.modelMats[prevId][prevInd]) * prevWorldPos;

    vec2 currUV = uvFromLocal(localPos.xyz, prevId, uCurrInd);
    prevUV = uvFromLocal(localPos.xyz, prevId, prevInd);

    reprojDelta = (currUV - prevUV) * vec2(noiseRes);
    reprojDelta = quantizePx(reprojDelta, 128.0);
  }

  vec2 prevAcc = imageLoad(uPrevAccTex, prevPx).xy;

  vec2 flow = vec2(0);
  if (uFlow) {
    vec2 flowDir = texelFetch(uPrevFlowTex, prevFullPx, 0).xy;

    flow = flowDir * uScrollSpeed;
    flow = quantizePx(flow, 32.0);
  }

  vec2 totalMove = prevAcc + reprojDelta + flow;

  vec2 intStep = trunc(totalMove);
  vec2 nextAcc = totalMove - intStep;

  imageStore(uPrevAccTex, prevPx, vec4(nextAcc, 0, 0));

  ivec2 targetPx = prevPx + ivec2(intStep);
  vec2 targetUV = (vec2(targetPx) + 0.5) / vec2(noiseRes);
  ivec2 targetFullPx = ivec2(round(targetUV * vec2(fullRes) - 0.5));

  if (targetPx.x < 0 || targetPx.x >= noiseRes.x || targetPx.y < 0 || targetPx.y >= noiseRes.y) return;

  int targetId = texelFetch(uCurrIdTex, targetFullPx, 0).r;
  if (targetId != prevId) return;

  //int asdf = texelFetch(uPrevIdTex, targetFullPx, 0).r;
  //if (asdf != prevId) {}

  uint myKey = uint(prevPx.x) + (uint(prevPx.y) << 16);
  uint oldKey = imageAtomicMin(uClaimTex, targetPx, myKey);
  if (myKey <= oldKey) {
    imageStore(uCurrNoiseTex, targetPx, vec4(prevNoise.r, 1, 0, 0));
    imageStore(uCurrAccTex, targetPx, vec4(nextAcc, 0, 0));
  }
}

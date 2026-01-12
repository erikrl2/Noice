#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rg8, binding = 0) uniform writeonly image2D uCurrNoiseTex;
layout(rg8, binding = 1) uniform readonly image2D uPrevNoiseTex;
layout(rg32f, binding = 2) uniform writeonly image2D uCurrAccTex;
layout(rg32f, binding = 3) uniform image2D uPrevAccTex;

layout(binding = 0) uniform sampler2D uFlowTex;
layout(binding = 1) uniform isampler2D uCurrIdTex;
layout(binding = 2) uniform isampler2D uPrevIdTex;
layout(binding = 3) uniform sampler2D uPrevLocalPosTex;

layout(std430, binding = 0) readonly buffer ObjectTransforms {
    mat4 modelMats[][2];
} b;

uniform mat4 uViewProj[2];

uniform float uScrollSpeed;
uniform bool uReproject;
uniform int uCurrInd;

vec2 uvFromLocal(vec3 localPos, int objID, int i) {
  vec4 worldPos = b.modelMats[objID][i] * vec4(localPos, 1);
  vec4 clip = uViewProj[i] * worldPos;
  if (clip.w <= 0.0) return vec2(-1);

  vec3 ndc = clip.xyz / clip.w;
  if (abs(ndc.x) > 1.0 || abs(ndc.y) > 1.0 || abs(ndc.z) > 1.0) return vec2(-1);

  return ndc.xy * 0.5 + 0.5;
}

void main() {
  ivec2 fullRes = textureSize(uFlowTex, 0);
  ivec2 noiseRes = imageSize(uPrevNoiseTex);

  ivec2 prevPx = ivec2(gl_GlobalInvocationID.xy);
  vec2 prevUV = (vec2(prevPx) + 0.5) / vec2(noiseRes);

  if (prevPx.x >= noiseRes.x || prevPx.y >= noiseRes.y) return;

  vec2 prevNoise = imageLoad(uPrevNoiseTex, prevPx).rg;
  if (prevNoise.g < 0.1) return; // not yet initialized
    
  ivec2 prevFullPx = ivec2(prevUV * vec2(fullRes));
  int prevId = texelFetch(uPrevIdTex, prevFullPx, 0).r;
  if (prevId < 0) {
    imageStore(uCurrNoiseTex, prevPx, vec4(prevNoise.r, 1, 0, 0));
    //imageStore(uCurrNoiseTex, prevPx, vec4(0.5, 1, 0, 0));
    return;
  }

  vec2 currUV = prevUV;
  vec2 reprojDelta = vec2(0);

  if (uReproject) {
    vec3 localPos = texelFetch(uPrevLocalPosTex, prevFullPx, 0).xyz;

    currUV = uvFromLocal(localPos, prevId, uCurrInd); // THIS RETURNS A WRONG RESULT
    prevUV = uvFromLocal(localPos, prevId, 1 - uCurrInd);

    if (currUV.x < 0.0 || prevUV.x < 0.0) return;

    reprojDelta = (currUV - prevUV) * vec2(noiseRes);

#if 0 // not needed with this new approach
    float eps = 1.0 / 1024.0;
    if (abs(reprojDelta.x) < eps) reprojDelta.x = 0.0;
    if (abs(reprojDelta.y) < eps) reprojDelta.y = 0.0;
#endif
  }

  vec2 flowDir = texelFetch(uFlowTex, ivec2(currUV * vec2(fullRes)), 0).xy;
  vec2 prevAcc = imageLoad(uPrevAccTex, prevPx).xy;

  vec2 flow = flowDir * uScrollSpeed;

  vec2 totalMove = prevAcc + reprojDelta + flow;
  vec2 intStep = trunc(totalMove);
  vec2 nextAcc = totalMove - intStep;

  imageStore(uPrevAccTex, prevPx, vec4(nextAcc, 0, 0));

  ivec2 targetPx = prevPx + ivec2(intStep);
  vec2 targetUV = (vec2(targetPx) + 0.5) / vec2(noiseRes);

  if (targetPx.x < 0 || targetPx.x >= noiseRes.x || targetPx.y < 0 || targetPx.y >= noiseRes.y) return;

  int targetId = texelFetch(uCurrIdTex, ivec2(targetUV * vec2(fullRes)), 0).r;
  if (targetId != prevId) {
    imageStore(uCurrNoiseTex, prevPx, vec4(prevNoise.r, 1, 0, 0));
    return;
  }

  imageStore(uCurrNoiseTex, targetPx, vec4(prevNoise.r, 1, 0, 0));
  imageStore(uCurrAccTex, targetPx, vec4(nextAcc, 0, 0));
}

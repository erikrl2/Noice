#version 430 core
layout(local_size_x = 16, local_size_y = 16) in;

layout(rg32f, binding = 0) uniform writeonly image2D  uCurrAccTex;
layout(rg16i, binding = 1) uniform writeonly iimage2D uCurrStepTex;
layout(rg32f, binding = 2) uniform readonly  image2D  uPrevAccTex;

layout(binding = 0) uniform isampler2D uCurrIdTex;
layout(binding = 1) uniform sampler2D  uCurrLocalPosTex;
layout(binding = 2) uniform sampler2D  uFlowPxPerUnitTex; // full-res px/world-unit in CURRENT frame

layout(std430, binding = 0) readonly buffer ObjectTransforms { mat4 modelMats[][2]; } b;
uniform mat4 uViewProj[2];
uniform int  uCurrInd;

uniform float uScrollSpeed;
uniform float uDt;
uniform float uDownscaleFactor;

vec2 uvFromWorld(vec3 worldPos, int i) {
  vec4 clip = uViewProj[i] * vec4(worldPos, 1.0);
  if (clip.w <= 0.0) return vec2(-1.0);
  vec3 ndc = clip.xyz / clip.w;
  if (abs(ndc.x) > 1.0 || abs(ndc.y) > 1.0 || abs(ndc.z) > 1.0) return vec2(-1.0);
  return ndc.xy * 0.5 + 0.5;
}
vec2 uvFromLocal(vec3 localPos, int objID, int frameInd) {
  vec3 worldPos = (b.modelMats[objID][frameInd] * vec4(localPos, 1.0)).xyz;
  return uvFromWorld(worldPos, frameInd);
}

vec2 samplePrevAccBilinear(vec2 prevUV, ivec2 noiseRes) {
  vec2 p = prevUV * vec2(noiseRes) - 0.5;
  ivec2 p0 = ivec2(floor(p));
  vec2 f = fract(p);

  ivec2 p00 = clamp(p0 + ivec2(0,0), ivec2(0), noiseRes - ivec2(1));
  ivec2 p10 = clamp(p0 + ivec2(1,0), ivec2(0), noiseRes - ivec2(1));
  ivec2 p01 = clamp(p0 + ivec2(0,1), ivec2(0), noiseRes - ivec2(1));
  ivec2 p11 = clamp(p0 + ivec2(1,1), ivec2(0), noiseRes - ivec2(1));

  vec2 a00 = imageLoad(uPrevAccTex, p00).xy;
  vec2 a10 = imageLoad(uPrevAccTex, p10).xy;
  vec2 a01 = imageLoad(uPrevAccTex, p01).xy;
  vec2 a11 = imageLoad(uPrevAccTex, p11).xy;

  return mix(mix(a00, a10, f.x), mix(a01, a11, f.x), f.y);
}

void main() {
  ivec2 noiseRes = imageSize(uCurrAccTex);
  ivec2 currPx = ivec2(gl_GlobalInvocationID.xy);
  if (currPx.x >= noiseRes.x || currPx.y >= noiseRes.y) return;

  ivec2 fullRes = textureSize(uCurrLocalPosTex, 0);
  vec2 currUV_n = (vec2(currPx) + 0.5) / vec2(noiseRes);
  ivec2 currFullPx = ivec2(currUV_n * vec2(fullRes));

  int currId = texelFetch(uCurrIdTex, currFullPx, 0).r;
  if (currId < 0) {
    imageStore(uCurrAccTex, currPx, vec4(0,0,0,0));
    imageStore(uCurrStepTex, currPx, ivec4(0,0,0,0));
    return;
  }

  vec3 currLocalPos = texelFetch(uCurrLocalPosTex, currFullPx, 0).xyz;

  // Surface backtrace to prev frame (this avoids "edge reset" due to mask mismatch)
  int prevInd = 1 - uCurrInd;
  vec2 prevUV = uvFromLocal(currLocalPos, currId, prevInd);
  if (prevUV.x < 0.0) {
    imageStore(uCurrAccTex, currPx, vec4(0,0,0,0));
    imageStore(uCurrStepTex, currPx, ivec4(0,0,0,0));
    return;
  }

  vec2 prevAcc = samplePrevAccBilinear(prevUV, noiseRes);

  // Flow move in NOISE pixels (computed in GBuffer in FULL pixels per world-unit)
  vec2 flowPxPerUnit_full = texelFetch(uFlowPxPerUnitTex, currFullPx, 0).xy;
  vec2 flowPxPerUnit_noise = flowPxPerUnit_full / uDownscaleFactor;
  vec2 flowMove = flowPxPerUnit_noise * (uScrollSpeed * uDt);

  vec2 totalMove = prevAcc + flowMove;

  ivec2 intStep = ivec2(trunc(totalMove));
  vec2 nextAcc  = totalMove - vec2(intStep);

  imageStore(uCurrAccTex, currPx, vec4(nextAcc, 0, 0));
  imageStore(uCurrStepTex, currPx, ivec4(intStep, 0, 0));
}

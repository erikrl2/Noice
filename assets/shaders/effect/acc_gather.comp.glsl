#version 430 core
layout(local_size_x = 16, local_size_y = 16) in;

layout(rg32f, binding = 0) uniform writeonly image2D uCurrAccTex;
layout(rg32f, binding = 1) uniform readonly  image2D uPrevAccTex;

layout(binding = 0) uniform isampler2D uCurrIdTex;
layout(binding = 1) uniform sampler2D  uMotionCurrTex;       // prevUV - currUV (UV units)
layout(binding = 2) uniform sampler2D  uFlowPxPerUnitTex;    // FULL-RES pixels per world-unit (vec2)

uniform float uScrollSpeed;   // world-units per second (or per frame if you already multiply)
uniform float uDt;            // seconds
uniform float uDownscaleFactor; // e.g. 2,4,... (fullRes/noiseRes)
uniform bool  uUseFlow;

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

  ivec2 fullRes = textureSize(uMotionCurrTex, 0);
  vec2 currUV = (vec2(currPx) + 0.5) / vec2(noiseRes);
  ivec2 currFullPx = ivec2(currUV * vec2(fullRes));

  int currId = texelFetch(uCurrIdTex, currFullPx, 0).r;
  if (currId < 0) {
    imageStore(uCurrAccTex, currPx, vec4(0,0,0,0));
    return;
  }

  vec2 motionUV = texelFetch(uMotionCurrTex, currFullPx, 0).xy; // prevUV - currUV
  vec2 prevUV = currUV + motionUV;

  vec2 prevAcc = samplePrevAccBilinear(prevUV, noiseRes);

  vec2 flowMoveNoisePx = vec2(0.0);
  if (uUseFlow && uScrollSpeed != 0.0 && uDt != 0.0) {
    vec2 flowPxPerUnit_full = texelFetch(uFlowPxPerUnitTex, currFullPx, 0).xy; // full-res px/unit
    vec2 flowPxPerUnit_noise = flowPxPerUnit_full / uDownscaleFactor;
    flowMoveNoisePx = flowPxPerUnit_noise * (uScrollSpeed * uDt);
  }

  vec2 totalMove = prevAcc + flowMoveNoisePx;

  ivec2 intStep = ivec2(trunc(totalMove));
  vec2 nextAcc  = totalMove - vec2(intStep);

  imageStore(uCurrAccTex, currPx, vec4(nextAcc, 0, 0));
}

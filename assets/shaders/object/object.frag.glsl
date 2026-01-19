#version 430 core

in vec3 vPosLocal;
in vec3 vFlowLocal;

layout(location = 0) out vec2 oMotion;
layout(location = 1) out vec2 oFlowPxPerWorldUnit;
layout(location = 2) out vec3 oLocalPos;
layout(location = 3) out int  oObjectId;
layout(location = 4) out vec2 oCurrUV;

uniform int uObjectId;

uniform mat4 uModelCurr;
uniform mat4 uModelPrev;
uniform mat4 uViewProjCurr;
uniform mat4 uViewProjPrev;

uniform int uMotionMode;      // 0: raster curr, 1: raster prev (you already use this in vertex)
uniform vec2 uViewportPx;     // FULL-RES viewport size in pixels (width,height)

vec2 clipToUV(vec4 clip) {
  vec2 ndc = clip.xy / clip.w;
  return ndc * 0.5 + 0.5;
}

bool uvValid(vec2 uv) { return all(greaterThanEqual(uv, vec2(0.0))) && all(lessThanEqual(uv, vec2(1.0))); }

void main() {
  oLocalPos = vPosLocal;
  oObjectId = uObjectId;

  vec4 currClip = uViewProjCurr * (uModelCurr * vec4(vPosLocal, 1.0));
  vec4 prevClip = uViewProjPrev * (uModelPrev * vec4(vPosLocal, 1.0));

  if (currClip.w <= 0.0 || prevClip.w <= 0.0) {
    oMotion = vec2(0.0);
    oCurrUV = vec2(-1.0);
    oFlowPxPerWorldUnit = vec2(0.0);
    return;
  }

  vec2 currUV = clipToUV(currClip);
  vec2 prevUV = clipToUV(prevClip);

  oCurrUV = currUV;

  if (uMotionMode == 0) oMotion = prevUV - currUV; // curr-space
  else                 oMotion = currUV - prevUV; // prev-space

  // FlowPxPerWorldUnit in CURRENT frame:
  // Convert local dir to world dir (curr frame), take a small step, measure UV delta.
  vec3 worldPos = (uModelCurr * vec4(vPosLocal, 1.0)).xyz;
  vec3 worldDir = (uModelCurr * vec4(vFlowLocal, 0.0)).xyz;

  float dirLen = length(worldDir);
  if (dirLen < 1e-20) { oFlowPxPerWorldUnit = vec2(0.0); return; }
  worldDir /= dirLen;

  float eps = 1.0; // world units; will shrink if needed

  vec2 uv0 = currUV;
  vec2 uv1 = vec2(-1.0);

  // try shrink if goes invalid
  for (int k=0;k<8;k++) {
    vec4 c1 = uViewProjCurr * vec4(worldPos + worldDir * eps, 1.0);
    if (c1.w > 0.0) {
      uv1 = clipToUV(c1);
      if (uvValid(uv1)) break;
    }
    eps *= 0.5;
  }

  if (!uvValid(uv0) || !uvValid(uv1) || eps < 1e-6) {
    oFlowPxPerWorldUnit = vec2(0.0);
    return;
  }

  vec2 dPx = (uv1 - uv0) * uViewportPx;
  oFlowPxPerWorldUnit = dPx / eps;
}

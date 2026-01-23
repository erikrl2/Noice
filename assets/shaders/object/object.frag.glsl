#version 430 core

in vec3 vPosWorld;
in vec3 vDirWorld;

layout(location = 0) out vec2 oFlow;
layout(location = 1) out int oObjectId;

uniform int uObjectId;

uniform mat4 uViewProj;
uniform vec2 uViewportSize;

vec2 uvFromWorld(vec3 worldPos) {
  vec4 clip = uViewProj * vec4(worldPos, 1);
  if (clip.w <= 0.0) return vec2(-1);
  vec3 ndc = clip.xyz / clip.w;
  return ndc.xy * 0.5 + 0.5;
}

void main() {
  float eps = 1.0;

  vec2 uv0 = uvFromWorld(vPosWorld);
  vec2 uv1;

  for (int k = 0; k < 8; k++) {
    uv1 = uvFromWorld(vPosWorld + normalize(vDirWorld) * eps);
    if (uv1.x > 0) break;
    eps *= 0.5;
  }

  vec2 dPx = (uv1 - uv0) * uViewportSize;

  oFlow = dPx / eps;

  oObjectId = uObjectId;
}

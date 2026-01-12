#version 430 core

in vec3 vPosLocal;
in vec3 vPosWorld;
in vec3 vDirWorld;

layout(location = 0) out vec2 oFlow;
layout(location = 1) out vec3 oLocalPos;
layout(location = 2) out int oObjectId;

uniform vec2 uViewportSize;
uniform mat4 uViewproj;
uniform int uObjectId;

void main() {
  oLocalPos = vPosLocal;
  oObjectId = uObjectId;

  const float epsWorld = 0.002;

  vec4 c0 = uViewproj * vec4(vPosWorld, 1);
  vec4 c1 = uViewproj * vec4(vPosWorld + vDirWorld * epsWorld, 1);

  vec2 ndc0 = c0.xy / c0.w;
  vec2 ndc1 = c1.xy / c1.w;

  vec2 dNdc = ndc1 - ndc0;
  vec2 dPx  = dNdc * 0.5 * uViewportSize;

  oFlow = dPx / epsWorld;
}

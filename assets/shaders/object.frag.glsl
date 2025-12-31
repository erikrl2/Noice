#version 430 core

in vec3 vPosWorld;
in vec3 vDirWorld;

layout(location = 0) out vec2 oDir;

uniform mat4 uViewproj;
uniform vec2 uViewportSize;

uniform bool uUniformFlow;
uniform mat4 uView;

void main() {
  if (uUniformFlow) {
    oDir = normalize(uView * vec4(vDirWorld, 0)).xy;
    return;
  }

  const float epsWorld = 0.002;

  vec4 c0 = uViewproj * vec4(vPosWorld, 1);
  vec4 c1 = uViewproj * vec4(vPosWorld + vDirWorld * epsWorld, 1);

  vec2 ndc0 = c0.xy / c0.w;
  vec2 ndc1 = c1.xy / c1.w;

  vec2 dNdc = ndc1 - ndc0;
  vec2 dPx  = dNdc * 0.5 * uViewportSize;

  oDir = dPx / epsWorld;

  oDir /= 150.0;
}

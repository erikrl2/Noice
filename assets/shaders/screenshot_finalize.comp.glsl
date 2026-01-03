#version 430 core

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(r16f, binding = 0) uniform readonly image2D uSumTex;
layout(r16f, binding = 1) uniform readonly image2D uDiffSumTex;

layout(r8, binding = 2) uniform writeonly image2D uOutTex;

uniform int uMethod;
uniform int uFrames;
uniform float uGain;
uniform float uGamma;

float applyGamma(float x, float g) {
  x = clamp(x, 0.0, 1.0);
  if (abs(g - 1.0) < 1e-6) return x;
  return pow(x, 1.0 / max(0.0001, g));
}

void main() {
  ivec2 px = ivec2(gl_GlobalInvocationID.xy);

  float v = 0.0;

  if (uMethod == 0) {
    float sum = imageLoad(uSumTex, px).r;
    v = sum / float(max(1, uFrames));
  } else {
    float acc = imageLoad(uDiffSumTex, px).r;
    v = acc / float(max(1, uFrames - 1));
  }

  v *= uGain;
  v = clamp(v, 0.0, 1.0);
  v = applyGamma(v, uGamma);

  imageStore(uOutTex, px, vec4(v, 0, 0, 0));
}

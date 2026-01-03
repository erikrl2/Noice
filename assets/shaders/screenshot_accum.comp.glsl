#version 430 core

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

uniform sampler2D uSourceTex;

layout(r16f, binding = 0) uniform image2D uSumTex;

layout(r16f, binding = 1) uniform image2D uDiffSumTex;
layout(r8, binding = 2) uniform image2D uPrevTex;

uniform int uMethod;
uniform int uFrameIndex;

float srcR(ivec2 px) {
  return texelFetch(uSourceTex, px, 0).r;
}

void main() {
  ivec2 px = ivec2(gl_GlobalInvocationID.xy);

  float cur = texelFetch(uSourceTex, px, 0).r;

  if (uMethod == 0) {
    float sum = imageLoad(uSumTex, px).r;
    imageStore(uSumTex, px, vec4(sum + cur, 0, 0, 0));
  } else {
    float prev = imageLoad(uPrevTex, px).r;
    float diff = abs(cur - prev);

    float acc = imageLoad(uDiffSumTex, px).r;
    imageStore(uDiffSumTex, px, vec4(acc + diff, 0, 0, 0));

    imageStore(uPrevTex, px, vec4(cur, 0, 0, 0));
  }
}

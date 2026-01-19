#version 430 core
layout(local_size_x = 16, local_size_y = 16) in;

layout(rg8, binding = 0) uniform writeonly image2D uCurrNoiseTex;
layout(rg8, binding = 1) uniform readonly  image2D uPrevNoiseTex;

layout(binding = 0) uniform isampler2D uPrevIdTex;
layout(binding = 1) uniform isampler2D uCurrIdTex;
layout(binding = 2) uniform sampler2D  uMotionPrevTex; // motion = currUV - prevUV, prev-space

ivec2 pxFromUv(vec2 uv, ivec2 res) {
  ivec2 px = ivec2(round(uv * vec2(res) - 0.5));
  return clamp(px, ivec2(0), res - ivec2(1));
}

void main() {
  ivec2 noiseRes = imageSize(uPrevNoiseTex);
  ivec2 prevPx = ivec2(gl_GlobalInvocationID.xy);
  if (prevPx.x >= noiseRes.x || prevPx.y >= noiseRes.y) return;

  vec2 prevNoise = imageLoad(uPrevNoiseTex, prevPx).rg;
  if (prevNoise.g < 0.1) return;

  ivec2 fullRes = textureSize(uMotionPrevTex, 0);
  vec2 prevUV = (vec2(prevPx) + 0.5) / vec2(noiseRes);
  ivec2 prevFullPx = ivec2(prevUV * vec2(fullRes));

  int prevId = texelFetch(uPrevIdTex, prevFullPx, 0).r;
  if (prevId < 0) {
    if (texelFetch(uCurrIdTex, prevFullPx, 0).r < 0) {
      imageStore(uCurrNoiseTex, prevPx, vec4(prevNoise.r, 1, 0, 0));
      return;
    }
  }

  vec2 motion = texelFetch(uMotionPrevTex, prevFullPx, 0).xy; // currUV - prevUV
  vec2 currUV = prevUV + motion;

  ivec2 targetPx = pxFromUv(currUV, noiseRes);

  // validate target in current frame
  ivec2 targetFullPx = ivec2(((vec2(targetPx)+0.5)/vec2(noiseRes)) * vec2(fullRes));
  targetFullPx = clamp(targetFullPx, ivec2(0), fullRes - ivec2(1));
  int targetId = texelFetch(uCurrIdTex, targetFullPx, 0).r;
  if (targetId != prevId) return;

  imageStore(uCurrNoiseTex, targetPx, vec4(prevNoise.r, 1, 0, 0));
}

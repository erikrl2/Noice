#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rg8, binding = 0) uniform writeonly image2D uCurrNoiseTex;
layout(rg8, binding = 1) uniform readonly image2D uPrevNoiseTex;
layout(rg16f, binding = 2) uniform writeonly image2D uCurrAccTex;
layout(rg16f, binding = 3) uniform image2D uPrevAccTex;

uniform sampler2D uFlowTex;
uniform sampler2D uCurrDepthTex;
uniform sampler2D uPrevDepthTex;

uniform mat4 uInvPrevProj;
uniform mat4 uInvPrevView;
uniform mat4 uInvPrevModel;
uniform mat4 uCurrModel;
uniform mat4 uCurrViewProj;

uniform float uScrollSpeed;
uniform bool uReproject;

void main() {
  ivec2 prevPx = ivec2(gl_GlobalInvocationID.xy);
  ivec2 size = imageSize(uPrevNoiseTex);
  if (prevPx.x >= size.x || prevPx.y >= size.y) return;

  vec2 prevNoise = imageLoad(uPrevNoiseTex, prevPx).rg;
  if (prevNoise.g < 0.1) return;

  vec2 prevUV = (vec2(prevPx) + 0.5) / vec2(size);
    
  vec2 currUV;
  ivec2 currPx;
  vec3 currNDC;

  if (uReproject) {
    float prevDepth = texture(uPrevDepthTex, prevUV).r;
    if (prevDepth >= 1.0) {
      imageStore(uCurrNoiseTex, prevPx, vec4(prevNoise.r, 1, 0, 0));
      return;
    }

    vec3 prevNDC = vec3(prevUV, prevDepth) * 2.0 - 1.0;
    vec4 prevClip = vec4(prevNDC, 1);
    vec4 prevViewPos = uInvPrevProj * prevClip;
    prevViewPos /= prevViewPos.w;
    vec4 prevWorldPos = uInvPrevView * vec4(prevViewPos.xyz, 1);

    vec4 localPos = uInvPrevModel * prevWorldPos;
    vec4 currWorldPos = uCurrModel * localPos;
    vec4 currClip = uCurrViewProj * currWorldPos;

    if (currClip.w <= 0.0) return;

    currNDC = currClip.xyz / currClip.w;

    if (abs(currNDC.x) > 1.0 || abs(currNDC.y) > 1.0 || abs(currNDC.z) > 1.0) return;

    currUV = currNDC.xy * 0.5 + 0.5;
    currPx = ivec2(currUV * vec2(size));
  } else {
    currUV = prevUV;
    currPx = prevPx;
  }


  vec2 flowDir = texture(uFlowTex, currUV).xy;
  vec2 prevAcc = imageLoad(uPrevAccTex, prevPx).xy;

  vec2 flow = flowDir * uScrollSpeed;

  vec2 totalMove = prevAcc + flow;
  vec2 intStep = trunc(totalMove); // or floor(totalMove + 0.5 * sign(totalMove));

  vec2 nextAcc = totalMove - intStep;
  imageStore(uPrevAccTex, prevPx, vec4(nextAcc, 0, 0));

  ivec2 targetPx = currPx + ivec2(intStep);
  vec2 targetUV = (vec2(targetPx) + 0.5) / vec2(size);


  if (targetPx.x < 0 || targetPx.x >= size.x || targetPx.y < 0 || targetPx.y >= size.y) return;

  if (uReproject) {
    float targetDepth = texture(uCurrDepthTex, targetUV).r;
    if (targetDepth >= 1.0) return;

    float depthDiff = abs(currNDC.z - (targetDepth * 2.0 - 1.0));
    if (depthDiff > 0.05) return;
  }


  imageStore(uCurrNoiseTex, targetPx, vec4(prevNoise.r, 1, 0, 0));
  imageStore(uCurrAccTex, targetPx, vec4(nextAcc, 0, 0));
}

#version 430 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rg8, binding = 0) uniform image2D uCurrNoiseTex;
layout(rg8, binding = 1) uniform writeonly image2D uPrevNoiseTex;
layout(rg32f, binding = 2) uniform image2D uCurrAccTex;
layout(rg32f, binding = 3) uniform image2D uPrevAccTex;

layout(binding = 0) uniform isampler2D uCurrIdTex;

uniform uint uSeed;

uint hash(uvec2 p) {
  uint h = p.x * 374761393u + p.y * 668265263u + uSeed;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h;
}

float rng(uvec2 p) {
  return float(hash(p)) * (1.0 / 4294967296.0);
}

void main() {
  ivec2 px = ivec2(gl_GlobalInvocationID.xy);
  ivec2 size = imageSize(uCurrNoiseTex);
  if (px.x >= size.x || px.y >= size.y) return;

  vec2 noise = imageLoad(uCurrNoiseTex, px).rg;
  if (noise.g < 0.5) {
    int id = texelFetch(uCurrIdTex, px, 0).r;

    float noiseVal = step(0.5, rng(uvec2(px)));
    imageStore(uCurrNoiseTex, px, vec4(noiseVal, 1, 0, 0));

    vec2 acc = vec2(0.0);
#if 1
    acc = imageLoad(uPrevAccTex, px).xy;
#else
    if (id >= 0) {
      // try inherit from already-filled neighbors of SAME object in curr frame
      ivec2 offs[4] = ivec2[4](ivec2(-1,0), ivec2(1,0), ivec2(0,-1), ivec2(0,1));
      for (int i=0;i<4;i++){
        ivec2 q = px + offs[i];
        if (q.x<0||q.y<0||q.x>=size.x||q.y>=size.y) continue;

        int qid = texelFetch(uCurrIdTex, q, 0).r;
        if (qid != id) continue;

        vec2 qnoise = imageLoad(uCurrNoiseTex, q).rg;
        if (qnoise.g < 0.5) continue; // neighbor not valid yet

        acc = imageLoad(uCurrAccTex, q).xy;
        break;
      }
    }
#endif

    imageStore(uCurrAccTex, px, vec4(acc, 0, 0));
  }

  imageStore(uPrevNoiseTex, px, vec4(0));
  imageStore(uPrevAccTex, px, vec4(0));
}

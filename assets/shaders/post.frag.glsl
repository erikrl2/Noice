#version 430 core

layout(location = 0) out vec4 oColor;

uniform sampler2D uScreenTex;
uniform vec2 uResolution;
uniform bool uShowVectors;

void main() {
  vec2 v = texture(uScreenTex, gl_FragCoord.xy / uResolution).rg;

  if (!uShowVectors) {
    oColor = vec4(v.r, v.r, v.r, 1);
  } else {
    if (v != vec2(0)) v = normalize(v);
    oColor = vec4(v * 0.5 + 0.5, 0, 1);
  }
}

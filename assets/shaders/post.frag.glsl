#version 430 core

layout(location = 0) out vec4 oColor;

layout(binding = 0) uniform sampler2D uSrcTex;

uniform vec2 uFullResolution;
uniform bool uShowVectors;

void main() {
  vec3 v = texture(uSrcTex, gl_FragCoord.xy / uFullResolution).rgb;

  if (!uShowVectors) {
    oColor = vec4(v.r, v.r, v.r, 1);
  } else {
    oColor = vec4(v * 0.5 + 0.5, 1);
  }
}

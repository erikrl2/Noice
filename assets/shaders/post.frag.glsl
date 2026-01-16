#version 430 core

layout(location = 0) out vec4 oColor;

layout(binding = 0) uniform sampler2D uSrcTex;

uniform vec2 uFullResolution;
uniform bool uShowVectors;

void main() {
  vec4 v = texture(uSrcTex, gl_FragCoord.xy / uFullResolution);

  if (!uShowVectors) {
    oColor = vec4(v.r, v.r, v.r, 1);
  } else {
    // TODO: make distinction
    //oColor = vec4(v.xyz * 0.5 + 0.5, 1); // flow
    oColor = vec4(v.xy * 0.5 + 0.5, 0, 1); // acc
  }
}

#version 430 core

layout(location = 0) out vec4 oColor;

layout(binding = 0) uniform sampler2D uSrcTex;

uniform vec2 uFullResolution;
uniform bool uShowVectors = false;
uniform bool uShowScreenshot = false;
uniform bool uNormalizeVectors = false;

void main() {
  vec4 v = texture(uSrcTex, gl_FragCoord.xy / uFullResolution);

  if (uShowVectors) {
    vec2 dir = v.xy;
    if (uNormalizeVectors && dir != vec2(0)) dir = normalize(dir);
    oColor = vec4(dir * 0.5 + 0.5, 0, 1);
  } else if (uShowScreenshot) {
    oColor = vec4(v.rgb, 1);
  } else {
    oColor = vec4(v.r, v.r, v.r, 1);
  }
}

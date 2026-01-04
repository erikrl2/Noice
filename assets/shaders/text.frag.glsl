#version 430 core

in vec2 vUV;

layout(location = 0) out vec2 oDir;

uniform sampler2D uFontAtlas;
uniform vec2 uDir;

void main() {
  float a = texture(uFontAtlas, vUV).r;
  float cov = step(0.5, a);
  //float cov = smoothstep(0.4, 0.6, a);

  if (cov <= 0.0) discard;

  oDir = uDir * cov;
}

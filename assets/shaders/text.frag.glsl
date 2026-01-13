#version 430 core

in vec2 vUV;

layout(location = 0) out vec2 oDir;
layout(location = 1) out int oId;

layout(binding = 0) uniform sampler2D uFontAtlas;

uniform vec2 uDir;

void main() {
  float a = texture(uFontAtlas, vUV).r;

  if (a < 0.5) discard;

  oDir = uDir;
  oId = 1;
}

#version 430 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

out vec2 vUV;

uniform vec2 uScreenSize;

void main() {
  vUV = aUV;

  vec2 ndc = (aPos / uScreenSize) * 2.0 - 1.0;
  ndc.y = -ndc.y;

  gl_Position = vec4(ndc, 0, 1);
}

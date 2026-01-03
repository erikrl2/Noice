#version 430 core

layout(location = 0) out vec2 oDir;

uniform sampler2D uCanvas;

void main() {
  vec2 dir = texelFetch(uCanvas, ivec2(gl_FragCoord.xy), 0).xy;
  if (dir != vec2(0)) dir = normalize(vec2(dir.x, -dir.y));
  oDir = dir * 20.0;
}

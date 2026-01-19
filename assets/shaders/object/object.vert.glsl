#version 430 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aTangent;

out vec3 vPosLocal;
out vec3 vFlowLocal;

uniform mat4 uMvp;

void main() {
  vPosLocal = aPos;
  vFlowLocal = aTangent;

  gl_Position = uMvp * vec4(aPos, 1);
}

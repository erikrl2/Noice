#version 430 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aTangent;

out vec3 vPosLocal;
out vec3 vFlowLocal;

uniform mat4 uModelCurr;
uniform mat4 uModelPrev;
uniform mat4 uViewProjCurr;
uniform mat4 uViewProjPrev;

uniform int uMotionMode;

void main() {
  vPosLocal = aPos;
  vFlowLocal = aTangent;

  if (uMotionMode == 0) {
    gl_Position = uViewProjCurr * (uModelCurr * vec4(aPos, 1));
  } else {
    gl_Position = uViewProjPrev * (uModelPrev * vec4(aPos, 1));
  }
}

#version 430 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aTangent;

out vec3 vPosWorld;
out vec3 vDirWorld;

uniform mat4 uViewProj;
uniform mat4 uModel;

void main() {
  vPosWorld = (uModel * vec4(aPos, 1)).xyz;
  vDirWorld = mat3(uModel) * aTangent;

  gl_Position = uViewProj * vec4(vPosWorld, 1);
}

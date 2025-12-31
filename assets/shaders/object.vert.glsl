#version 430 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aTangent;

out vec3 vPosWorld;
out vec3 vDirWorld;

uniform mat4 uModel;
uniform mat4 uViewproj;

void main() {
  vPosWorld = (uModel * vec4(aPos, 1)).xyz;
  vDirWorld = normalize(mat3(uModel) * aTangent);

  gl_Position = uViewproj * vec4(vPosWorld, 1);
}

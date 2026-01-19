#version 430 core

in vec3 vPosLocal;
in vec3 vFlowLocal;

layout(location = 0) out vec3 oFlow;
layout(location = 1) out vec3 oLocalPos;
layout(location = 2) out int oObjectId;

uniform int uObjectId;

void main() {
  oFlow = vFlowLocal;
  oLocalPos = vPosLocal;
  oObjectId = uObjectId;
}

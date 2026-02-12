#version 430 core

layout(local_size_x = 1, local_size_y = 1) in;

layout(r32ui, binding = 5) uniform readonly uimage2D uNeedsJfaFlag;

layout(std430, binding = 1) buffer IndirectArgs { uint gx, gy, gz; } uArgs; 

uniform uint uGroupsX;
uniform uint uGroupsY;

void main() {
  if (imageLoad(uNeedsJfaFlag, ivec2(0, 0)).r != 0u) {
    uArgs.gx = uGroupsX;
    uArgs.gy = uGroupsY;
    uArgs.gz = 1u;
  } else {
    uArgs.gx = 0u;
    uArgs.gy = 0u;
    uArgs.gz = 0u;
  }
}

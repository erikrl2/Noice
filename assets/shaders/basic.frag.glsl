#version 430 core

in vec3 t;

layout(location=0) out vec2 outRG;

uniform mat4 viewproj;
uniform mat4 view;

void main() {
    // outRG = normalize(viewproj * vec4(t, 0)).xy;
    outRG = normalize(view * vec4(t, 0)).xy;
}


// TODO
// in vec2 flowPx;
// layout(location=0) out vec2 outRG;

// void main() {
//     float len2 = dot(flowPx, flowPx);
//     if (len2 < 1e-10) outRG = vec2(0);
//     else outRG = flowPx * inversesqrt(len2); // nur Richtung; Speed machst du später
// }

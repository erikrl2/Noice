#version 430 core

layout(location=0) in vec3 pos;
layout(location=1) in vec3 tangent;

out vec3 t;

uniform mat4 viewproj;
uniform mat4 model;
uniform mat4 normalMatrix;

void main() {
    gl_Position = viewproj * model * vec4(pos, 1);
    t = mat3(normalMatrix) * tangent;
}


// TODO
// uniform mat4 model;
// uniform mat4 viewproj;
// uniform vec2 viewportSize; // (width,height)
// uniform float epsWorld;    // z.B. 0.002 * bboxDiagonal

// out vec2 flowPx; // Pixelspace direction (nicht normiert)

// void main() {
//     vec3 pWorld = (model * vec4(pos, 1.0)).xyz;
//     vec3 tWorld = normalize(mat3(model) * tangent);

//     vec4 c0 = viewproj * vec4(pWorld, 1.0);
//     vec4 c1 = viewproj * vec4(pWorld + tWorld * epsWorld, 1.0);

//     vec2 ndc0 = c0.xy / c0.w;
//     vec2 ndc1 = c1.xy / c1.w;

//     // NDC [-1,1] -> Pixel
//     vec2 dNdc = ndc1 - ndc0;
//     flowPx = dNdc * 0.5 * viewportSize;

//     gl_Position = c0;
// }

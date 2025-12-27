#version 430 core

in VS_OUT {
    vec3 pWorld;
    vec3 tWorld;
} fs_in;

out vec2 outRG;

uniform mat4 viewproj;
uniform vec2 viewportSize;

uniform bool uniformFlow;
uniform mat4 view;

void main() {
    if (uniformFlow) {
        outRG = normalize(view * vec4(fs_in.tWorld, 0)).xy;
        return;
    }

    const float epsWorld = 0.002;

    vec4 c0 = viewproj * vec4(fs_in.pWorld, 1.0);
    vec4 c1 = viewproj * vec4(fs_in.pWorld + fs_in.tWorld * epsWorld, 1.0);

    vec2 ndc0 = c0.xy / c0.w;
    vec2 ndc1 = c1.xy / c1.w;

    vec2 dNdc = ndc1 - ndc0;
    vec2 dPx  = dNdc * 0.5 * viewportSize;

    outRG = dPx / epsWorld;

    outRG /= 150.0;
}

#version 430 core

out vec2 oDir;

uniform sampler2D uCanvas;

void main() {
    vec2 dir = texelFetch(uCanvas, ivec2(gl_FragCoord.xy), 0).xy;

    if (dir == vec2(0)) {
        oDir = vec2(0);
    }
    else {
        oDir = normalize(vec2(dir.x, -dir.y));
    }
}

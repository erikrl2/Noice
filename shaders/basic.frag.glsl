#version 430 core

in vec3 n;
out vec4 fragColor;

uniform mat4 viewproj;
uniform vec3 color;
uniform bool outputNormal;

void main() {
    if (outputNormal) {
#if 0
        vec3 up = abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
        vec3 t = normalize(viewproj * vec4(cross(up, n), 0.0)).xyz;
#else
        vec3 t = normalize(viewproj * vec4(n, 0.0)).xyz;
        t.xy = t.yx * vec2(-1.0, 1.0);
#endif
        fragColor = vec4(t.xy * 0.5 + 0.5, 0.5, 1.0);
    } else {
        fragColor = vec4(color, 1.0);
    }
}
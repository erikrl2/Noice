#version 420 core

out vec4 FragColor;

uniform sampler2D prevTex;
uniform sampler2D objectTex;

uniform float doXor;

void main() {
    vec2 texSize = vec2(textureSize(prevTex, 0));
    vec2 uv = gl_FragCoord.xy / texSize;

    vec3 a = texture(prevTex, uv).rgb;
    vec3 b = texture(objectTex, uv).rgb;

    if (doXor == 0.0) {
        FragColor = vec4(a, 1);
        return;
    }
#if 1
    FragColor = vec4(abs(a - b), 1);
#else
    uvec3 r = uvec3(a * 255.0) ^ uvec3(b * 255.0);
    FragColor = vec4(vec3(r), 1);
#endif
}

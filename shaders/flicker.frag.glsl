#version 430 core

out vec4 FragColor;

uniform sampler2D prevTex;
uniform sampler2D objectTex;

uniform bool doXor;

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(prevTex, 0));

    vec3 a = texture(prevTex, uv).rgb;
    vec3 b = texture(objectTex, uv).rgb;

    if (doXor) {
        FragColor = vec4(abs(a - b), 1);
    } else {
        FragColor = vec4(a, 1);
    }
}

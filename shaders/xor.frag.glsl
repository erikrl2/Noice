#version 330 core

in vec2 uv;

out vec4 FragColor;

uniform sampler2D prevTex;
uniform sampler2D objectTex;

vec4 texture_nearest_texel(sampler2D tex, vec2 uv) {
    ivec2 ts = textureSize(tex, 0);
    vec2 coord = (floor(uv * vec2(ts)) + vec2(0.5)) / vec2(ts);
    return texture(tex, coord);
}

void main() {
    vec3 a = texture_nearest_texel(prevTex, uv).rgb;
    vec3 b = texture_nearest_texel(objectTex, uv).rgb;

    FragColor = vec4(abs(a - b), 1);
}

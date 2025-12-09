#version 330 core

in vec2 uv;

out vec4 FragColor;

uniform sampler2D prevTex;
uniform sampler2D objectTex;

uniform float doXor;
uniform vec2 scrollDir;
uniform float rand;

float rng() {
    return step(0.5, fract(sin(dot(gl_FragCoord.xy + rand, vec2(12.9898, 78.233))) * 43758.5453));
}

void main() {
    vec3 objColor = texture(objectTex, uv).rgb;

    if (objColor != vec3(1, 0, 0)) { // flicker wireframe (TODO: also scroll?)
        vec3 noiseColor = texture(prevTex, uv).rgb;
        FragColor = vec4(doXor == 1.0 ? abs(objColor - noiseColor) : noiseColor, 1);
    }
    else { // scroll faces
        vec2 texSize = textureSize(prevTex, 0);
        vec2 nextUV = (uv * texSize - scrollDir) / texSize;

        bool hitsWireframe = (texture(objectTex, nextUV).rgb == vec3(1.0));
        if (hitsWireframe) {
            FragColor = vec4(vec3(rng()), 1);
        } else {
            FragColor = vec4(texture(prevTex, nextUV).rgb, 1);
        }
    }
}


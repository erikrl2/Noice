#version 430 core

in vec2 vUV;

// layout(location=0) out vec2 outRG;
out vec4 FragColor;

uniform sampler2D uFontAtlas;

uniform vec2 uDir;
uniform float uThreshold;
uniform float uSoftness;
uniform int uPremultiply;

void main()
{
    float a = texture(uFontAtlas, vUV).r;

    float soft = max(uSoftness, 1e-6);
    float cov = smoothstep(uThreshold, uThreshold + soft, a);

    if (cov <= 0.0)
        discard;

    // outRG = (uPremultiply != 0) ? (uDir * cov) : uDir;
    FragColor = vec4((uPremultiply != 0) ? (uDir * cov) : uDir, 0, 0);
}

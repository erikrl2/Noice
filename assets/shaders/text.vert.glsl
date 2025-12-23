#version 430 core

layout(location=0) in vec2 aPos; // pixel coords
layout(location=1) in vec2 aUV;

out vec2 vUV;

uniform vec2 uScreenSize;

void main()
{
    vUV = aUV;

    // pixel -> NDC
    vec2 ndc = (aPos / uScreenSize) * 2.0 - 1.0;

    // flip y (we build y down in pixels)
    ndc.y = -ndc.y;

    gl_Position = vec4(ndc, 0.0, 1.0);
}

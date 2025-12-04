#version 330 core

layout(location=0) in vec2 quadPos;
layout(location=1) in vec2 quadUV;

out vec2 uv;

void main(){
    gl_Position = vec4(quadPos, 0, 1);
    uv = quadUV;
}

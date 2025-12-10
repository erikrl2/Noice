#version 420 core

layout(location=0) in vec2 quadPos;

void main(){
    gl_Position = vec4(quadPos, 0, 1);
}

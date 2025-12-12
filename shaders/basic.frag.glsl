#version 430 core

in vec3 worldNormal;
out vec4 fragColor;

uniform vec3 color;
uniform bool outputNormal;

void main(){
    if (outputNormal) {
        fragColor = vec4(worldNormal * 0.5 + 0.5, 1.0);
    } else {
        fragColor = vec4(color, 1.0);
    }
}
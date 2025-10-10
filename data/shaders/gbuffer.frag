#version 330 core

in vec3 normalV;
in vec4 colorV;

layout(location = 0) out vec4 colorG;
layout(location = 1) out vec4 normalG;

void main() {
    colorG = colorV;
    normalG = vec4(normalV * 0.5 + 0.5, gl_FragCoord.z);
}

#version 330 core

in vec3 normalV;
in vec4 colorV;

out layout(location = 0) vec4 colorG;
out layout(location = 1) vec4 normalG;

void main() {
    colorG = colorV;
    normalG = vec4(normalV * 0.5 + 0.5, gl_FragCoord.z);
}

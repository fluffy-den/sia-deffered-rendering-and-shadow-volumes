#version 330 core

in vec4 vtx_position;

void main() {
    gl_Position = vec4((vtx_position.xy - 0.5) * 2.0, vtx_position.zw);
}

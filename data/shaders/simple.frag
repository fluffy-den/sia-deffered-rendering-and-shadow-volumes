#version 330 core

uniform vec3 light_col;

out vec4 out_color;

void main() { out_color = vec4(light_col, 1.f); }

// void main() {
//   // DEBUG: Front face is green, back face is red
//   if (gl_FrontFacing) {
//     out_color = vec4(0.0, 1.0, 0.0, 1.0);
//   } else {
//     out_color = vec4(1.0, 0.0, 0.0, 1.0);
//   }
// }


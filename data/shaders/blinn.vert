#version 410 core

uniform mat4 projection_matrix;
uniform mat4 view_matrix;
uniform mat4 model_matrix;
uniform mat3 normal_matrix;

in vec4 vtx_position;
in vec3 vtx_normal;
in vec3 vtx_color;

out vec3 vertexV;
out vec3 normalV;
out vec3 colorV;

void main() {
  vec4 view_vtx = view_matrix * model_matrix * vtx_position;
  vertexV = view_vtx.xyz;
  gl_Position = projection_matrix * view_vtx;

  colorV = vtx_color;
  normalV = normalize(normal_matrix * vtx_normal);
}

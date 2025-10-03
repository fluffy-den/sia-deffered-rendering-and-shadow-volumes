#version 330 core

uniform mat4 projection_matrix;
uniform mat4 view_matrix;
uniform mat4 model_matrix;
uniform mat3 normal_matrix;

uniform float specular_coef;

in vec4 vtx_position;
in vec3 vtx_normal;
in vec3 vtx_color;

out vec3 normalV;
out vec4 colorV;

void main()
{
    vec4 view_vtx = view_matrix * model_matrix * vtx_position;
    vec4 vertexV = view_vtx.xyzw;
    gl_Position = projection_matrix * view_vtx;

    colorV = vec4(vtx_color, specular_coef);
    normalV = normalize(normal_matrix * vtx_normal).xyz;
}

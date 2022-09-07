#version 410 core

uniform mat4 view_matrix;
uniform mat3 normal_matrix;
uniform vec4 light_pos;
uniform vec3 light_col;
uniform float specular_coef;

in vec3 vertexV;
in vec3 normalV;
in vec3 colorV;

out vec4 out_color;

vec3 phong(vec3 n, vec3 l, vec3 v, vec3 diffuse_color, vec3 specular_color,
           float specular_coef, float exponent, vec3 light_color) {
  float dotProd = max(0, dot(n, l));
  vec3 diffuse = diffuse_color * dotProd;
  vec3 specular = vec3(0.);
  if (dotProd > 0) {
    // Blinn-Phong
    vec3 h = normalize(l + v);
    specular = specular_color * pow(max(0, dot(h, n)), exponent * 4);
  }
  return 0.1 * diffuse_color + // ambiant color
         0.9 * (diffuse + specular_coef * specular) * light_color;
}

void main() {
  vec3 l = vec3(light_pos) - vertexV;
  out_color = vec4(phong(normalize(normalV), normalize(l), -normalize(vertexV),
                         colorV.rgb, vec3(1.0), specular_coef, 5,
                         light_col / max(1, dot(l, l))),
                   1);
}

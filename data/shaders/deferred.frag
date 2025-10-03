#version 330 core

uniform mat4 inv_projection_matrix;
uniform vec4 light_pos;
uniform vec3 light_col;

uniform sampler2D color_sampler;
uniform sampler2D normal_sampler;

uniform float is_first;

uniform vec2 resolution;

out vec4 out_color;

vec3 phong(vec3 n, vec3 l, vec3 v, vec3 diffuse_color, vec3 specular_color,
    float specular_coef, float exponent, vec3 light_color) {
    float dot_prod = max(0, dot(n, l));
    vec3 diffuse = diffuse_color * dot_prod;
    vec3 specular = vec3(0.);
    if (dot_prod > 0) {
        // Blinn-Phong
        vec3 h = normalize(l + v);
        specular = specular_color * pow(max(0, dot(h, n)), exponent * 4);
    }
    return is_first * 0.1 * diffuse_color + // ambiant color
        0.9 * (diffuse + specular_coef * specular) * light_color;
}

vec3 VSPositionFromDepth(vec2 texcoord, float z)
{
    // Get x/w and y/w from the viewport position
    vec4 positionNDC = vec4(2 * vec3(texcoord, z) - 1, 1.f);
    // Transform by the inverse projection matrix
    vec4 positionVS = inv_projection_matrix * positionNDC;
    // Divide by w to get the view-space position
    return positionVS.xyz / positionVS.w;
}

void main() {
    vec2 fragcoord = gl_FragCoord.xy / resolution.xy;

    vec4 in_color_s = texture(color_sampler, fragcoord);
    vec4 in_normal_s = texture(normal_sampler, fragcoord);
    vec3 in_position = VSPositionFromDepth(fragcoord, in_normal_s.a);

    vec3 normal = normalize(in_normal_s.xyz * 2.0 - 1.0);
    vec3 l = vec3(light_pos) - in_position;

    float specular_coef = in_color_s.a;

    out_color = vec4(phong(normal, normalize(l), -normalize(in_position),
                in_color_s.rgb, vec3(1.0), specular_coef, 5,
                light_col / max(1, dot(l, l))),
            1);
}

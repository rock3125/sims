#version 330 core

in vec3 v_world_normal;
in vec3 v_world_pos;
in vec2 v_texcoord;

out vec4 frag_color;

uniform vec3 u_light_dir;
uniform vec3 u_light_color;
uniform vec3 u_ambient;
uniform vec3 u_base_color;
uniform int  u_has_texture;
uniform sampler2D u_texture;

void main() {
    vec3 N = normalize(v_world_normal);
    vec3 L = normalize(u_light_dir);
    float diff = max(dot(N, L), 0.0);

    vec3 albedo = u_base_color;
    if (u_has_texture == 1) {
        albedo *= texture(u_texture, v_texcoord).rgb;
    }

    vec3 color = albedo * (u_ambient + u_light_color * diff);
    frag_color = vec4(color, 1.0);
}

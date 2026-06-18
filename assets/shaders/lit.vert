#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord;

uniform mat4 u_view_proj;
uniform mat4 u_model;

out vec3 v_world_normal;
out vec3 v_world_pos;
out vec2 v_texcoord;

void main() {
    vec4 world = u_model * vec4(a_position, 1.0);
    gl_Position = u_view_proj * world;
    v_world_normal = mat3(u_model) * a_normal;
    v_world_pos = world.xyz;
    v_texcoord = a_texcoord;
}

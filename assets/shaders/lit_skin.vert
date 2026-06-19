#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord;
layout(location = 3) in ivec4 a_bone_ids;
layout(location = 4) in vec4  a_bone_weights;

uniform mat4 u_view_proj;
uniform mat4 u_model;
#define SIMS_MAX_BONES 128
uniform mat4 u_bone_matrices[SIMS_MAX_BONES];

out vec3 v_world_normal;
out vec3 v_world_pos;
out vec2 v_texcoord;

void main() {
    mat4 skin =
        u_bone_matrices[a_bone_ids.x] * a_bone_weights.x +
        u_bone_matrices[a_bone_ids.y] * a_bone_weights.y +
        u_bone_matrices[a_bone_ids.z] * a_bone_weights.z +
        u_bone_matrices[a_bone_ids.w] * a_bone_weights.w;

    vec4 skinned_pos = skin * vec4(a_position, 1.0);
    vec3 skinned_nrm = mat3(skin) * a_normal;

    vec4 world = u_model * skinned_pos;
    gl_Position = u_view_proj * world;
    v_world_normal = mat3(u_model) * skinned_nrm;
    v_world_pos = world.xyz;
    v_texcoord = a_texcoord;
}

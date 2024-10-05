#version 450

#extension GL_EXT_buffer_reference : require

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;

layout(std430, buffer_reference, buffer_reference_align = 8) readonly buffer ModelsPtr
{
    mat4 data[];
};

struct SceneData
{
    mat4 view;
    mat4 projection;
};

layout(std430, buffer_reference, buffer_reference_align = 128) readonly buffer SceneDataPtr
{
    SceneData data[];
};

layout (std430, push_constant) uniform Constants
{
    SceneDataPtr scene_data;
    ModelsPtr models;
} constants;

void main() {
    gl_Position = 
        constants.scene_data.data[0].projection * 
        constants.scene_data.data[0].view * 
        constants.models.data[gl_InstanceIndex] * 
            vec4(position, 1.0);
}
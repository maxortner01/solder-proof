#version 450

#extension GL_EXT_buffer_reference : require

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;

struct Instance
{
    mat4 model, normal;
};

layout(std430, buffer_reference, buffer_reference_align = 8) readonly buffer ModelsPtr
{
    Instance data[];
};

struct SceneData
{
    mat4 view;
    mat4 projection;
};

layout(std430, buffer_reference, buffer_reference_align = 8) readonly buffer SceneDataPtr
{
    SceneData data[];
};

struct Light
{
    vec3 position;
    float r;
    float g;
    float b;
};

layout(std430, buffer_reference, buffer_reference_align = 8) readonly buffer LightsPtr
{
    Light data[];
};

layout (std430, push_constant) uniform Constants
{
    SceneDataPtr scene_data;
    ModelsPtr models;
    LightsPtr lights;
    uint scene_index;
    uint light_count;
} constants;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outPosition;

void main() {

    gl_Position = 
        constants.scene_data.data[constants.scene_index].projection * 
        constants.scene_data.data[constants.scene_index].view * 
        constants.models.data[gl_InstanceIndex].model * 
            vec4(position, 1.0);

    outColor = vec4(1.0, 1.0, 1.0, 1.0);

    outNormal = normalize((constants.models.data[gl_InstanceIndex].normal * vec4(normal, 1.0)).xyz);
    outPosition = (constants.models.data[gl_InstanceIndex].model * vec4(position, 1.0)).xyz;
}
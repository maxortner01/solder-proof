#version 450

#extension GL_EXT_buffer_reference : require

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;
layout(location = 3) in vec2 tex_coords;

struct Instance
{
    mat4 model, normal;
    uint lit;
    uint test[15];
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

layout(std430, buffer_reference, buffer_reference_align = 8) readonly buffer InstancePtr
{
    uint data[];
};

layout (std140, push_constant) uniform Constants
{
    SceneDataPtr scene_data;
    ModelsPtr models;
    LightsPtr lights;
    InstancePtr instances;
    uint scene_index;
    uint light_count;
    uint offset;
    uint enable_lighting;
} constants;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outPosition;
layout(location = 3) out vec3 outViewPos;
layout(location = 4) out vec2 outTexCoords;
layout(location = 5) out flat uint lit;

void main() {
    uint index = gl_InstanceIndex + constants.offset;
    uint instance_index = constants.instances.data[index];

    gl_Position = 
        constants.scene_data.data[constants.scene_index].projection * 
        constants.scene_data.data[constants.scene_index].view * 
        constants.models.data[instance_index].model * 
            vec4(position, 1.0);

    lit = uint(constants.enable_lighting == 1 && constants.models.data[instance_index].lit == 1);

    outColor = color;
    outTexCoords = tex_coords;
    outNormal = (constants.models.data[instance_index].normal * vec4(normal, 1.0)).xyz;
    outPosition = (constants.models.data[instance_index].model * vec4(position, 1.0)).xyz;
    outViewPos = (constants.scene_data.data[constants.scene_index].view * vec4(0, 0, 0, 1)).xyz * -1.0;
}
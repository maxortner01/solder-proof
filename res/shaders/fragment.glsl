#version 450

#extension GL_EXT_buffer_reference : require

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

struct Position
{
    float x, y, z;
};

vec3 get_pos(Position p)
{
    return vec3(p.x, p.y, p.z);
}

struct Color
{
    float r, g, b;
};

vec3 get_color(Color c)
{
    return vec3(c.r, c.g, c.b);
}

struct Light
{
    float intensity;
    Position position;
    Color color;
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
    uint descriptor_present;
} constants;

layout(set = 0, binding = 0) uniform sampler samplers[1];
layout(set = 0, binding = 1) uniform texture2D textures[];

layout(location = 0) out vec4 gAlbedoSpec;
layout(location = 1) out vec4 gPosition;
layout(location = 2) out vec4 gNormal;

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 position;
layout(location = 3) in vec3 view_pos;
layout(location = 4) in vec2 tex_coords;

void main() {
    gPosition = vec4(position, constants.enable_lighting);
    gNormal = vec4(normalize(normal), 1);
    if (constants.descriptor_present == 1)
        gAlbedoSpec = texture(sampler2D(textures[0], samplers[0]), tex_coords);
    else
        gAlbedoSpec = vec4(inColor.xyz, 1.0);
}
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

layout (std140, push_constant) uniform Constants
{
    SceneDataPtr scene_data;
    ModelsPtr models;
    LightsPtr lights;
    uint scene_index;
    uint light_count;
} constants;

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 position;
layout(location = 3) in vec3 view_pos;
layout(location = 4) in vec2 tex_coords;

void main() {
    outColor = vec4(0, 0, 0, 1);
    for (int i = 0; i < constants.light_count; i++)
    {
        vec3 r = position - get_pos( constants.lights.data[i].position );

        vec3 viewDir = normalize(position - view_pos);
        vec3 reflectDir = normalize(reflect(-r, normal));
        
        outColor += vec4(
            get_color(constants.lights.data[i].color) * 
            (constants.lights.data[i].intensity * // Diffuse
            min(max(dot(normalize(r), normal), 0.0) / dot(r, r), 1.0)
            + 0.08 * pow(max(dot(viewDir, reflectDir), 0.0), 8)), // Specular 
        1);
    }
    outColor = texture(textures[0], tex_coords) * inColor * (outColor);
    outColor.w = 1.0;
}
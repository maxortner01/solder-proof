#version 450

#extension GL_EXT_buffer_reference : require

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
    LightsPtr lights;
    uint light_count;
    Position view_pos;
} constants;

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 inTexCoords;

layout(set = 0, binding = 0) uniform sampler samplers[2];
layout(set = 0, binding = 1) uniform texture2D textures[];

void main() {
    vec4 position_sample = texture(sampler2D(textures[1], samplers[0]), inTexCoords);

    uint enable_lighting = uint(position_sample.w);

    vec3 inColor  = texture(sampler2D(textures[0], samplers[0]), inTexCoords).xyz;
    vec3 position = position_sample.xyz;
    vec3 normal   = texture(sampler2D(textures[2], samplers[0]), inTexCoords).xyz;

    if (enable_lighting == 0) 
    {
        outColor = vec4(inColor, 1.0);
        return;
    }

    vec3 view_pos = get_pos(constants.view_pos);

    outColor = vec4(0, 0, 0, 1);
    for (int i = 0; i < constants.light_count; i++)
    {
        vec3 r = get_pos( constants.lights.data[i].position ) - position;

        vec3 viewDir = normalize(position - view_pos);
        vec3 halfwayDir = normalize(r + viewDir);
        
        outColor += vec4(
            get_color(constants.lights.data[i].color) * 
            (constants.lights.data[i].intensity * // Diffuse
            min(max(dot(normalize(r), normal), 0.0) / dot(r, r), 1.0)
            + 0.08 * pow(max(dot(normal, halfwayDir), 0.0), 0.5)), // Specular 
        1);
    }
    outColor = vec4(inColor.xyz, 1.0) * outColor;
}
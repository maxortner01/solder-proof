#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout (std140, push_constant) uniform Constants
{
    float exposure;
    uint scene_index;
} constants;

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 inTexCoords;

layout(set = 0, binding = 0) uniform sampler samplers[2];
layout(set = 0, binding = 1) uniform texture2D textures[];

void main() {
    const float gamma = 2.2;
    vec3 color_in = texture(sampler2D(textures[0 * 4 + 3], samplers[0]), inTexCoords * -1.0).xyz;
  
    // exposure tone mapping
    vec3 mapped = vec3(1.0) - exp(-color_in * constants.exposure);
    // gamma correction 
    mapped = pow(mapped, vec3(1.0 / gamma));
  
    outColor = vec4(mapped, 1.0);
}
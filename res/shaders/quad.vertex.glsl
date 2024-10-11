#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;
layout(location = 3) in vec2 tex_coords;

layout(location = 0) out vec2 outTexCoords;

void main() {
    gl_Position = vec4(position.xy, 0.0, 1.0);
    outTexCoords = tex_coords;
}
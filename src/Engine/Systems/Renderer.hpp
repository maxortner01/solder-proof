#pragma once

#include "../Component.hpp"

#include <midnight/midnight.hpp>

#include <flecs.h>

namespace Engine::System
{
    struct Renderer
    {
        struct InstanceData
        {
            mn::Math::Mat4<float> model, normal;
        };

        struct RenderData
        {
            mn::Math::Mat4<float> view, projection;
        };

        struct Light
        {
            float intensity;
            mn::Math::Vec3f position, color;
        };

        struct PushConstant
        {
            mn::Graphics::Buffer::gpu_addr scene_data, models, lights;
            uint32_t scene_index, light_count;
        };

        Renderer(flecs::world _world, const std::string& luafile);
        void render(mn::Graphics::RenderFrame& rf) const;

    private:
        std::shared_ptr<mn::Graphics::Image> surface;
        mn::Graphics::Texture box_texture;

        flecs::world world;

        flecs::query<const Component::Camera> camera_query;
        flecs::query<const Component::Light, const Component::Transform> light_query;
        flecs::query<const Component::Model, const Component::Transform> model_query;

        mutable mn::Graphics::Pipeline pipeline;
        mutable mn::Graphics::TypeBuffer<InstanceData> brother_buffer;
        mutable mn::Graphics::TypeBuffer<RenderData> scene_data;
        mutable mn::Graphics::TypeBuffer<Light> light_data;
    };
}
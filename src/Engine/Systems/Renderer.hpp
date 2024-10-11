#pragma once

#include "../Component.hpp"

#include <midnight/midnight.hpp>

#include <flecs.h>

namespace Engine::System
{
    struct Renderer
    {
        // Each model instance represented in the GPU
        struct InstanceData
        {
            mn::Math::Mat4<float> model, normal;
        };

        // The per-camera data representation in the GPU
        struct RenderData
        {
            mn::Math::Mat4<float> view, projection;
        };

        // The light information representation in the GPU
        struct Light
        {
            float intensity;
            mn::Math::Vec3f position, color;
        };

        // The push constants
        struct PushConstant
        {
            mn::Graphics::Buffer::gpu_addr scene_data, models, lights, instance_indices;
            uint32_t scene_index, light_count, offset;
        };

        struct GBufferPush
        {
            mn::Graphics::Buffer::gpu_addr lights;
            uint32_t light_count;
            mn::Math::Vec3f view_pos;
            float extra; // Stupid padding C vs. SPIR-V
        };

        // Renderer settings
        struct Settings
        {
            bool wireframe = false;
        } settings;

        // Images used to store geometry information
        struct GBuffer
        {
            mn::Math::Vec2u size;
            std::shared_ptr<mn::Graphics::Image> position, normal, color;

            GBuffer() = default;

            void rebuild(mn::Math::Vec2u size);
        };

        Renderer(flecs::world _world, const mn::Graphics::PipelineBuilder& builder);
        
        void render(mn::Graphics::RenderFrame& rf) const;

        void drawOverlay() const;

    private:
        flecs::world world;

        mutable std::vector<GBuffer> gbuffers;

        flecs::query<const Component::Camera> camera_query;
        flecs::query<const Component::Light, const Component::Transform> light_query;
        flecs::query<const Component::Model, const Component::Transform> model_query;

        std::shared_ptr<mn::Graphics::Mesh> quad_mesh;

        std::shared_ptr<mn::Graphics::Descriptor> descriptor;
        std::shared_ptr<mn::Graphics::Pipeline> pipeline, wireframe_pipeline, quad_pipeline;
        mutable mn::Graphics::TypeBuffer<InstanceData> brother_buffer;
        mutable mn::Graphics::TypeBuffer<uint32_t> instance_buffer;
        mutable mn::Graphics::TypeBuffer<RenderData> scene_data;
        mutable mn::Graphics::TypeBuffer<Light> light_data;
    };
}
#pragma once

#include "../Component.hpp"
#include "../../Util/Profiler.hpp"

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
            uint32_t lit;
            uint32_t test[15];
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
            uint32_t scene_index, light_count, offset, enable_lighting;
        };

        struct GBufferPush
        {
            mn::Graphics::Buffer::gpu_addr lights;
            uint32_t light_count;
            mn::Math::Vec3f view_pos;
            float extra; // Stupid padding C vs. SPIR-V
            // Should have a scene index variable here, 4 * scene_index is the start of
            // the corresponding images in the descriptor
        };

        struct HDRPush
        {
            float exposure;
            uint32_t index;
            // We want to read from scene_index * 4 + 3
        };

        // Renderer settings
        struct Settings
        {
            bool wireframe = false;
            bool bounding_boxes = false;
        } settings;

        // Images used to store geometry information
        struct GBuffer
        {
            mn::Math::Vec2u size;
            std::shared_ptr<mn::Graphics::Image> gbuffer, hdr_surface;

            GBuffer() = default;

            void rebuild(mn::Math::Vec2u size);
        };

        Renderer(flecs::world _world);
        
        void render(mn::Graphics::RenderFrame& rf) const;

        void drawOverlay() const;

    private:
        flecs::world world;

        bool cull(const BoundingBox& aabb, const mn::Math::Mat4<float>& model, const Component::Transform& transform, const Component::Camera& camera) const;

        mutable std::size_t total_instance_count;

        mutable std::vector<GBuffer> gbuffers;

        std::shared_ptr<Util::Profiler> profiler;

        flecs::query<const Component::Camera> camera_query;
        flecs::query<const Component::Light, const Component::Transform> light_query;
        flecs::query<const Component::Model, const Component::Transform> model_query;

        std::shared_ptr<mn::Graphics::Mesh> quad_mesh;
        //std::shared_ptr<Engine::Model> cube_model;

        std::shared_ptr<mn::Graphics::Descriptor::Layout> gbuffer_descriptor_layout;
        std::shared_ptr<mn::Graphics::Descriptor> gbuffer_descriptor;
        std::shared_ptr<mn::Graphics::Pipeline> hdr_pipeline, quad_pipeline;

        mutable mn::Graphics::TypeBuffer<InstanceData> brother_buffer;
        mutable mn::Graphics::TypeBuffer<uint32_t> instance_buffer;
        mutable mn::Graphics::TypeBuffer<RenderData> scene_data;
        mutable mn::Graphics::TypeBuffer<Light> light_data;
    };
}
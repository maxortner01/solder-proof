#include "Renderer.hpp"

namespace Engine::System
{
    Renderer::Renderer(flecs::world _world, const std::string& luafile) : 
        world{_world},
        model_query(_world.query_builder<const Component::Model, const Component::Transform>()
            .cached()
            .order_by<Component::Model>(
                [](flecs::entity_t e1, const Component::Model *d1, flecs::entity_t e2, const Component::Model *d2) {
                    return (d1->model.get() > d2->model.get()) - (d1->model.get() < d2->model.get());
                }
            )
            .build()),
        camera_query(_world.query_builder<const Component::Camera>()
            .cached()
            .build()),
        light_query(_world.query_builder<const Component::Light, const Component::Transform>()
            .cached()
            .build()),
        pipeline(mn::Graphics::PipelineBuilder()
            .fromLua(RES_DIR, luafile)
            .setPushConstantObject<PushConstant>()
            .build())
    {   
        surface = std::make_shared<mn::Graphics::Image>(
            mn::Graphics::ImageFactory()
                .addAttachment<mn::Graphics::Image::Color>(mn::Graphics::Image::B8G8R8A8_UNORM, { 1280U / 2, 720U / 2 })
                .build()
        );
    }

    void Renderer::render(mn::Graphics::RenderFrame& rf) const
    {
        using namespace mn;
        
        //if (!model_query.count() || !camera_query.count() || !light_query.count()) return;

        struct ModelRep
        {
            std::size_t offset;
            std::shared_ptr<Graphics::Model> model;
        };

        std::vector<ModelRep> offsets;

        // Resize our GPU buffers
        scene_data.resize(camera_query.count()); 
        light_data.resize(light_query.count());
        brother_buffer.resize(model_query.count());

        // we can keep handle the descriptor set here as well
        // go through the unique models and push the texture

        std::size_t it = 0;
        model_query.each(
            [&](const Component::Model& model, const Component::Transform& transform)
            {
                if (!offsets.size() || offsets.back().model != model.model)
                    offsets.push_back(ModelRep{ .offset = it, .model = model.model });

                brother_buffer[it  ].model = Math::scale(transform.scale) * 
                    Math::rotation<float>(transform.rotation) * 
                    Math::translation(transform.position);
                
                brother_buffer[it++].normal = Math::rotation<float>(transform.rotation);
            });

        it = 0;
        camera_query.each(
            [&](flecs::entity e, const Component::Camera& camera)
            {
                const auto& attach = camera.surface->getAttachment<Graphics::Image::Color>();
                rf.image_stack.push(camera.surface);
                scene_data[it  ].view = ( e.has<Component::Transform>() ?
                    Math::translation(e.get<Component::Transform>()->position) :
                    Math::identity<4, float>()
                );
                scene_data[it++].projection = Math::perspective((float)Math::x(attach.size) / (float)Math::y(attach.size), camera.FOV, camera.near_far);
            });

        it = 0;
        light_query.each(
            [&](const Component::Light& light, const Component::Transform& transform)
            {
                light_data[it  ].position  = transform.position;
                light_data[it  ].intensity = light.intensity;
                light_data[it++].color    = light.color;
            });

        const auto size = rf.image_stack.size();
        for (uint32_t j = 0; j < size; j++)
        {
            rf.clear({ 0.f, 0.f, 0.f });
            rf.startRender();
            for (std::size_t i = 0; i < offsets.size(); i++)
            {
                // Calculate count
                const auto instances = ( i == offsets.size() - 1 ?
                    brother_buffer.size() - offsets[i].offset :
                    offsets[i + 1].offset - offsets[i].offset
                );

                rf.setPushConstant(pipeline, PushConstant {
                    .scene_index = j, 
                    .light_count = static_cast<uint32_t>(light_data.size()),
                    .lights      = light_data.getAddress(),
                    .scene_data  = scene_data.getAddress(),
                    .models      = reinterpret_cast<void*>( reinterpret_cast<std::size_t>(brother_buffer.getAddress()) + offsets[i].offset )
                });

                rf.draw(pipeline, offsets[i].model, instances);
            }

            rf.endRender();
            rf.image_stack.pop();
        }
    }
}
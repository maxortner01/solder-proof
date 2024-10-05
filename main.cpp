#include <Engine/Application.hpp>
#include <iostream>

#include <flecs.h>

template<class...Fs>
struct overloaded : Fs... {
  using Fs::operator()...;
};

template<class...Fs>
overloaded(Fs...) -> overloaded<Fs...>;

namespace Component
{
    struct Transform
    {
        mn::Math::Vec3f position, scale;
        mn::Math::Vec3<mn::Math::Angle> rotation;
    };

    struct Model
    {
        std::shared_ptr<mn::Graphics::Model> model;
    };

    struct Camera
    {
        mn::Math::Angle FOV;
        mn::Math::Vec2f near_far;
        std::shared_ptr<mn::Graphics::Image> surface;

        static Camera make(mn::Math::Vec2u size, mn::Math::Angle FOV, mn::Math::Vec2f nf)
        {
            Camera c;
            c.FOV = FOV;
            c.near_far = nf;
            c.surface = std::make_shared<mn::Graphics::Image>(
                mn::Graphics::ImageFactory()
                    .addAttachment<mn::Graphics::Image::Color>(mn::Graphics::Image::B8G8R8A8_UNORM, size)
                    .addAttachment<mn::Graphics::Image::DepthStencil>(mn::Graphics::Image::DF32_SU8, size)
                    .build()
            );
            return c;
        }
    };

    struct Light
    {
        mn::Math::Vec3f color;
        float intensity;
    };
}

namespace System
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

        Renderer(flecs::world _world, const std::string& luafile) : 
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

        void render(mn::Graphics::RenderFrame& rf) const
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

    private:
        std::shared_ptr<mn::Graphics::Image> surface;

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

struct MainScene : Engine::Scene
{
    struct SceneData
    {
        mn::Math::Mat4<float> view, projection; 
    };

    flecs::entity camera, light, light2;
    flecs::world world;
    double total_time = 0.0;

    System::Renderer renderer;
    std::shared_ptr<mn::Graphics::Model> model;

    std::vector<flecs::entity> entities;

    MainScene() :
        renderer(world, "/shaders/main.lua"),
        model(std::make_shared<mn::Graphics::Model>(mn::Graphics::Model::fromFrame([]()
        {
            mn::Graphics::Model::Frame frame;
            frame.vertices = {
                { { -1.f,  1.f, 1.f }, { 0.f, 0.f, 1.f } },
                { { -1.f, -1.f, 1.f }, { 0.f, 0.f, 1.f } },
                { {  1.f, -1.f, 1.f }, { 0.f, 0.f, 1.f } },
                { {  1.f,  1.f, 1.f }, { 0.f, 0.f, 1.f } },

                { { -1.f,  1.f, -1.f }, { 0.f, 0.f, -1.f } },
                { { -1.f, -1.f, -1.f }, { 0.f, 0.f, -1.f } },
                { {  1.f, -1.f, -1.f }, { 0.f, 0.f, -1.f } },
                { {  1.f,  1.f, -1.f }, { 0.f, 0.f, -1.f } },

                { { 1.f, -1.f,  1.f }, { 1.f, 0.f, 0.f } },
                { { 1.f, -1.f, -1.f }, { 1.f, 0.f, 0.f } },
                { { 1.f,  1.f, -1.f }, { 1.f, 0.f, 0.f } },
                { { 1.f,  1.f,  1.f }, { 1.f, 0.f, 0.f } },

                { { -1.f, -1.f,  1.f }, { -1.f, 0.f, 0.f } },
                { { -1.f, -1.f, -1.f }, { -1.f, 0.f, 0.f } },
                { { -1.f,  1.f, -1.f }, { -1.f, 0.f, 0.f } },
                { { -1.f,  1.f,  1.f }, { -1.f, 0.f, 0.f } },

                { {  1.f, 1.f, -1.f }, { 0.f, 1.f, 0.f } },
                { { -1.f, 1.f, -1.f }, { 0.f, 1.f, 0.f } },
                { { -1.f, 1.f,  1.f }, { 0.f, 1.f, 0.f } },
                { {  1.f, 1.f,  1.f }, { 0.f, 1.f, 0.f } },

                { {  1.f, -1.f, -1.f }, { 0.f, -1.f, 0.f } },
                { { -1.f, -1.f, -1.f }, { 0.f, -1.f, 0.f } },
                { { -1.f, -1.f,  1.f }, { 0.f, -1.f, 0.f } },
                { {  1.f, -1.f,  1.f }, { 0.f, -1.f, 0.f } },
            };

            frame.indices = 
            { 
                0 + 0, 1 + 0, 2 + 0, 2 + 0, 3 + 0, 0 + 0,
                0 + 4, 1 + 4, 2 + 4, 2 + 4, 3 + 4, 0 + 4,
                0 + 8, 1 + 8, 2 + 8, 2 + 8, 3 + 8, 0 + 8,
                0 + 12, 1 + 12, 2 + 12, 2 + 12, 3 + 12, 0 + 12,
                0 + 16, 1 + 16, 2 + 16, 2 + 16, 3 + 16, 0 + 16,
                0 + 20, 1 + 20, 2 + 20, 2 + 20, 3 + 20, 0 + 20,
            };

            return frame;
        }())))
    {   
        for (int i = 0; i < 1000; i++)
        {
            auto e = world.entity();
            e.set(Component::Model{ .model = model });
            e.set(Component::Transform{ .position = 
                { 
                    ((rand() % 1000) / 500.f - 1.f) *  40.f, 
                    ((rand() % 1000) / 500.f - 1.f) *  40.f, 
                    ((rand() % 1000) / 500.f - 1.f) * -40.f - 10.f
                },
                .scale = { 1.f, 1.f, 1.f }
            });
            entities.push_back(e);
        }

        camera = world.entity();
        camera.set(Component::Camera::make({ 1280U / 2, 720U / 2 }, mn::Math::Angle::degrees(90), { 0.1f, 100.f }));
        camera.set(Component::Transform{  });

        light = world.entity();
        light.set(Component::Transform{ .position = { 0.f, 5.f, 0.f } });
        light.set(Component::Light{ .color = { 0.f, 1.f, 0.f }, .intensity = 20.f });

        light2 = world.entity();
        light2.set(Component::Transform{ .position = { 0.f, -5.f, 0.f } });
        light2.set(Component::Light{ .color = { 1.f, 0.f, 0.f }, .intensity = 100.f });
    }

    Engine::Scene::Replace
    update(double dt) override
    {
        using namespace mn::Math;

        //std::cout << 1.0 / dt << "\n";

        total_time += dt;

        float _x = 1.f;
        for (const auto& e : entities)
        {
            z(e.get_mut<Component::Transform>()->rotation) += Angle::degrees(5.0 * dt);
            x(e.get_mut<Component::Transform>()->rotation) += Angle::degrees(5.0 * _x * dt);
            _x += 0.01f;
        }

        y(light2.get_mut<Component::Transform>()->position) = 40.f * sin(2.5f * total_time);
        x(light.get_mut<Component::Transform>()->position) = 40.f * sin(2.5f * total_time);
        z(light.get_mut<Component::Transform>()->position) = 40.f * cos(2.5f * total_time);
        //std::cout << x(light.get_mut<Component::Transform>()->position) << "\n";

        z(camera.get_mut<Component::Transform>()->position) -= 2.f * dt;

        //return { ( total_time >= 2.0 ) };
        return { done };
    }

    void
    render(mn::Graphics::RenderFrame& rf) const override
    {
        rf.clear({ 0.f, 0.f, 0.f });
        
        renderer.render(rf);

        rf.blit(camera.get<Component::Camera>()->surface, rf.image);
    }

    void 
    poll(mn::Graphics::Event& e) override
    {
        std::visit(overloaded{
            [](const auto&) {},
            [this](const mn::Graphics::Event::Quit&)
            {
                done = true;
            }
        }, e.event);
    }

    bool done = false;
};

int main()
{
    Engine::Application app({ 1280U, 720U });
    app.emplace_scene<MainScene>();
    app.run();
}
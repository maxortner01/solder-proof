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
}

namespace System
{
    struct Renderer
    {
        struct RenderData
        {
            mn::Math::Mat4<float> view, projection;
        };

        struct PushConstant
        {
            mn::Graphics::Buffer::gpu_addr scene_data, models;
        };

        Renderer(flecs::world _world, const std::string& luafile) : 
            world{_world},
            query(_world.query_builder<const Component::Model, const Component::Transform>()
                .order_by<Component::Model>(
                    [](flecs::entity_t e1, const Component::Model *d1, flecs::entity_t e2, const Component::Model *d2) {
                        return (d1->model.get() > d2->model.get()) - (d1->model.get() < d2->model.get());
                    }
                )
                .build()),
            pipeline(mn::Graphics::PipelineBuilder()
                .fromLua(RES_DIR, luafile)
                .setPushConstantObject<PushConstant>()
                .build())
        {   
            scene_data.resize(1); 
            scene_data[0].projection = mn::Math::perspective(1280.f / 720.f, mn::Math::Angle::degrees(90), { 0.1f, 100.f });
            scene_data[0].view       = mn::Math::translation<float>({ 0.f, 0.f, -5.f });
        }

        void render(mn::Graphics::RenderFrame& rf) const
        {
            using namespace mn;

            struct ModelRep
            {
                std::size_t offset;
                std::shared_ptr<Graphics::Model> model;
            };

            std::vector<ModelRep> offsets;
            std::vector<Math::Mat4<float>> models(query.count());

            std::size_t it = 0;
            query.each(
                [&](const Component::Model& model, const Component::Transform& transform)
                {
                    if (!offsets.size() || offsets.back().model != model.model)
                        offsets.push_back(ModelRep{ .offset = it, .model = model.model });

                    models[it++] = Math::translation<float>(transform.position);
                });
            
            // Copy our transform info into the GPU buffer
            if (!models.size()) return;
            brother_buffer.resize(models.size());
            std::copy(models.begin(), models.end(), &brother_buffer[0]);

            rf.startRender();
            for (std::size_t i = 0; i < offsets.size(); i++)
            {
                // Calculate count
                const auto instances = ( i == offsets.size() - 1 ?
                    models.size() - offsets[i].offset :
                    offsets[i + 1].offset - offsets[i].offset
                );

                rf.setPushConstant(pipeline, PushConstant {
                    .scene_data = scene_data.getAddress(),
                    .models     = reinterpret_cast<void*>( reinterpret_cast<std::size_t>(brother_buffer.getAddress()) + offsets[i].offset )
                });

                rf.draw(pipeline, offsets[i].model, instances);
            }
            rf.endRender();
        }

    private:
        flecs::world world;
        flecs::query<const Component::Model, const Component::Transform> query;
        mutable mn::Graphics::Pipeline pipeline;
        mutable mn::Graphics::TypeBuffer<mn::Math::Mat4<float>> brother_buffer;
        mutable mn::Graphics::TypeBuffer<RenderData> scene_data;
    };
}

struct MainScene : Engine::Scene
{
    struct SceneData
    {
        mn::Math::Mat4<float> view, projection; 
    };

    flecs::world world;
    double total_time = 0.0;

    System::Renderer renderer;
    std::shared_ptr<mn::Graphics::Model> model;

    MainScene() :
        renderer(world, "/shaders/main.lua"),
        model(std::make_shared<mn::Graphics::Model>(mn::Graphics::Model::fromFrame([]()
        {
            mn::Graphics::Model::Frame frame;
            frame.vertices = {
                { { -1.f,  1.f, -5.5f } },
                { { -1.f, -1.f, -5.5f } },
                { {  1.f, -1.f, -5.5f } },
                { {  1.f,  1.f, -5.5f } }
            };

            frame.indices = { 0, 1, 2, 2, 3, 0 };

            return frame;
        }())))
    {   
        for (int i = 0; i < 1000; i++)
        {
            auto e = world.entity();
            e.set(Component::Model{ .model = model });
            e.set(Component::Transform{ .position = 
                { 
                    ((rand() % 1000) / 500.f - 1.f) * 40.f, 
                    ((rand() % 1000) / 500.f - 1.f) * 40.f, 
                    -10.f 
                } 
            });
        }
    }

    Engine::Scene::Replace
    update(double dt) override
    {
        using namespace mn::Math;

        std::cout << 1.0 / dt << "\n";

        total_time += dt;

        //models[0] = rotation<float>({ Angle::degrees(0), Angle::degrees(0), Angle::degrees(total_time * 180.0) });

        return { ( total_time >= 2.0 ) };
    }

    void
    render(mn::Graphics::RenderFrame& rf) const override
    {
        rf.clear({ 0.f, 0.f, 0.f });
        
        // We can store *all* of each instance data in a large list on the GPU
        // Then we sort based off the model
        // Do a push constant here and offset models by the start of the current model
        // Draw the model

        // All of the models will be represented as a component with vec3 pos, vec<angle> rotation, vec3 scale
        // This massive component list will have a brother buffer on the GPU
        // Every frame we go through and convert each component to its corresponding model matrix then copy to (a vector then to) the brother buffer
        // We can even set the query to be sorted, so the entity will have a *model* component that has a unique ID for the raw model data (which we sort by)
        renderer.render(rf);
        /*
        rf.setPushConstant(pipeline, 
            PushConstant
            { 
                .models = models.getAddress(),
                .scene_data = scene_data.getAddress()
            });

        rf.startRender();
        rf.draw(pipeline, model);
        rf.endRender();*/
    }

    void 
    poll(mn::Graphics::Event& e) override
    {
        std::visit(overloaded{
            [](const auto&) {}
        }, e.event);
    }
};

int main()
{
    Engine::Application app({ 1280U, 720U });
    app.emplace_scene<MainScene>();
    app.run();
}
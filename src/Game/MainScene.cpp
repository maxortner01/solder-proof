#include "MainScene.hpp"

#include "../Engine/Component.hpp"

namespace Game
{
    template<class...Fs>
    struct overloaded : Fs... {
    using Fs::operator()...;
    };

    template<class...Fs>
    overloaded(Fs...) -> overloaded<Fs...>;

    MainScene::MainScene() :
        renderer(world, "/shaders/main.lua"),
        model(std::make_shared<mn::Graphics::Model>(mn::Graphics::Model::fromFrame([]()
        {
            mn::Graphics::Model::Frame frame;
            frame.vertices = {
                { { -1.f,  1.f, 1.f }, { 0.f, 0.f, 1.f }, { 1.f, 1.f, 1.f, 1.f }, { 0.f, 1.f } },
                { { -1.f, -1.f, 1.f }, { 0.f, 0.f, 1.f }, { 1.f, 1.f, 1.f, 1.f }, { 0.f, 0.f } },
                { {  1.f, -1.f, 1.f }, { 0.f, 0.f, 1.f }, { 1.f, 1.f, 1.f, 1.f }, { 1.f, 0.f } },
                { {  1.f,  1.f, 1.f }, { 0.f, 0.f, 1.f }, { 1.f, 1.f, 1.f, 1.f }, { 1.f, 1.f } },

                { { -1.f,  1.f, -1.f }, { 0.f, 0.f, -1.f }, { 1.f, 1.f, 1.f, 1.f }, { 0.f, 1.f } },
                { { -1.f, -1.f, -1.f }, { 0.f, 0.f, -1.f }, { 1.f, 1.f, 1.f, 1.f }, { 0.f, 0.f } },
                { {  1.f, -1.f, -1.f }, { 0.f, 0.f, -1.f }, { 1.f, 1.f, 1.f, 1.f }, { 1.f, 0.f } },
                { {  1.f,  1.f, -1.f }, { 0.f, 0.f, -1.f }, { 1.f, 1.f, 1.f, 1.f }, { 1.f, 1.f } },

                { { 1.f, -1.f,  1.f }, { 1.f, 0.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 0.f, 1.f } },
                { { 1.f, -1.f, -1.f }, { 1.f, 0.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 0.f, 0.f } },
                { { 1.f,  1.f, -1.f }, { 1.f, 0.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 1.f, 0.f } },
                { { 1.f,  1.f,  1.f }, { 1.f, 0.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 1.f, 1.f } },

                { { -1.f, -1.f,  1.f }, { -1.f, 0.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 0.f, 1.f } },
                { { -1.f, -1.f, -1.f }, { -1.f, 0.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 0.f, 0.f } },
                { { -1.f,  1.f, -1.f }, { -1.f, 0.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 1.f, 0.f } },
                { { -1.f,  1.f,  1.f }, { -1.f, 0.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 1.f, 1.f } },

                { {  1.f, 1.f, -1.f }, { 0.f, 1.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 1.f, 0.f } },
                { { -1.f, 1.f, -1.f }, { 0.f, 1.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 0.f, 0.f } },
                { { -1.f, 1.f,  1.f }, { 0.f, 1.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 0.f, 1.f } },
                { {  1.f, 1.f,  1.f }, { 0.f, 1.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 1.f, 1.f } },

                { {  1.f, -1.f, -1.f }, { 0.f, -1.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 1.f, 0.f } },
                { { -1.f, -1.f, -1.f }, { 0.f, -1.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 0.f, 0.f } },
                { { -1.f, -1.f,  1.f }, { 0.f, -1.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 0.f, 1.f } },
                { {  1.f, -1.f,  1.f }, { 0.f, -1.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 1.f, 1.f } },
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
        using namespace Engine;

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
        camera.set(Component::Camera::make({ 1280U / 2, 720U / 2 }, mn::Math::Angle::degrees(90), { 0.1f, 500.f }));
        camera.set(Component::Transform{  });

        light = world.entity();
        light.set(Component::Transform{ .position = { 0.f, 5.f, 0.f } });
        light.set(Component::Light{ .color = { 0.1f, 0.5f, 0.4f }, .intensity = 80.f });

        light2 = world.entity();
        light2.set(Component::Transform{ .position = { 0.f, -5.f, 0.f } });
        light2.set(Component::Light{ .color = { 1.f, 0.6f, 0.1f }, .intensity = 150.f });
    }

    Engine::Scene::Replace
    MainScene::update(double dt)
    {
        using namespace mn::Math;
        using namespace Engine;

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
    MainScene::render(mn::Graphics::RenderFrame& rf) const
    {
        using namespace Engine;

        rf.clear({ 0.f, 0.f, 0.f });
        
        renderer.render(rf);

        rf.blit(camera.get<Component::Camera>()->surface, rf.image);
    }

    void 
    MainScene::poll(mn::Graphics::Event& e)
    {
        std::visit(overloaded{
            [](const auto&) {},
            [this](const mn::Graphics::Event::Quit&)
            {
                done = true;
            }
        }, e.event);
    }
}
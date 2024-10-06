#pragma once

#include "../Engine/Application.hpp"
#include "../Engine/Systems/Renderer.hpp"

#include <flecs.h>

namespace Game
{
    struct MainScene : Engine::Scene
    {
        struct SceneData
        {
            mn::Math::Mat4<float> view, projection; 
        };

        flecs::entity camera, light, light2;
        flecs::world world;
        double total_time = 0.0;

        Engine::System::Renderer renderer;
        std::shared_ptr<mn::Graphics::Model> model;

        std::shared_ptr<mn::Graphics::Image> test_image;

        std::vector<flecs::entity> entities;

        MainScene();

        Engine::Scene::Replace
        update(double dt) override;

        void
        render(mn::Graphics::RenderFrame& rf) const override;

        void 
        poll(mn::Graphics::Event& e) override;

        bool done = false;
    };
}
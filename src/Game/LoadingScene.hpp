#pragma once

#include "../Engine/S&P.hpp"

namespace Game
{
    struct LoadingScene : Engine::Scene
    {
        struct Data
        {
            bool finished;
            std::string message;
        };

        LoadingScene(std::shared_ptr<mn::Graphics::Window> window, std::shared_ptr<Data> finished_loading);

        Engine::Scene::Replace update(double dt) override;
        void render(mn::Graphics::RenderFrame& rf) const override;
        void poll(mn::Graphics::Event& e) override;

    private:
        std::shared_ptr<Data> loading_finished;
    };
}
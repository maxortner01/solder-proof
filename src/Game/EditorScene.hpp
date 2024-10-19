#pragma once

#include <thread>

#include "../Engine/S&P.hpp"
#include "LoadingScene.hpp"

namespace Game
{
    struct EditorScene : Engine::Scene
    {
        EditorScene(std::shared_ptr<mn::Graphics::Window> window);
        ~EditorScene();

        Engine::Scene::Replace update(double dt) override;
        void render(mn::Graphics::RenderFrame& rf) const override;
        void poll(mn::Graphics::Event& e) override;

    private:
        mutable bool scene_hovered = false;

        std::unique_ptr<std::thread> loading_thread;
        std::shared_ptr<LoadingScene::Data> loading_finished;

        // Scene Data
        flecs::entity camera;
        std::unique_ptr<Engine::System::Renderer> renderer;

        // Camera surface (essentially)
        std::shared_ptr<mn::Graphics::Image> surface;

        float target_zoom;

        bool done;
    };
}
#pragma once

#include <midnight/midnight.hpp>

#include <vector>
#include <memory>
#include <chrono>

namespace Engine
{
    struct Scene
    {
        struct Replace
        {
            bool destroy = false;
            std::unique_ptr<Scene> next_scene;    
        };

        virtual ~Scene() = default;

        virtual Replace update(double dt) = 0;
        virtual void render(mn::Graphics::RenderFrame&) const = 0;
        virtual void poll(mn::Graphics::Event&) = 0;
    };

    struct Application
    {
        Application(mn::Math::Vec2u size) :
            window(size, "Hello")
        {

        }

        template<typename T, typename... Args>
            requires ( std::derived_from<T, Scene> )
        void emplace_scene(Args&&... args)
        {
            scenes.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
        }

        void run()
        {
            using namespace std::chrono;
            auto now = high_resolution_clock::now();

            while (scenes.size())
            {
                auto new_now = high_resolution_clock::now();
                const auto dt = duration<double>(new_now - now).count();
                now = new_now;

                auto new_scene = scenes.back()->update(dt);
                
                mn::Graphics::Event event;
                while (window.pollEvent(event))
                    scenes.back()->poll(event);


		        auto rf = window.startFrame();


                //rf.setPushConstant(pipeline, c);
                //rf.draw(pipeline, model);
                scenes.back()->render(rf);


		        window.endFrame(rf);

                if (new_scene.destroy)    scenes.pop_back();
                if (new_scene.next_scene) scenes.push_back(std::move(new_scene.next_scene));
            }

            window.finishWork();
        }

    private:
        mn::Graphics::Window window;
        std::vector<std::unique_ptr<Scene>> scenes;
    };
}
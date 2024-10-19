#pragma once

#include <midnight/midnight.hpp>

#include "ResourceManager.hpp"
#include "../Util/Profiler.hpp"

#include <vector>
#include <memory>
#include <chrono>

#include <imgui.h>
#include <flecs.h>

namespace Engine
{
    struct Scene
    {
        struct Replace
        {
            bool destroy = false;
            std::unique_ptr<Scene> next_scene;    
        };

        Scene(std::shared_ptr<mn::Graphics::Window> w) :
            window{w}
        {   }

        virtual ~Scene() = default;

        virtual Replace update(double dt) = 0;
        virtual void render(mn::Graphics::RenderFrame&) const = 0;
        virtual void poll(mn::Graphics::Event&) = 0;

    protected:
        flecs::entity createEntity(std::string name = "")
        {
            auto e = world.entity((name.length() ? name.c_str() : nullptr));
            entities.push_back(e);
            return e;
        }

        void renderOverlay() const;

        Engine::ResourceManager res;
        flecs::world world;

        // Needed only for overlay
        std::vector<flecs::entity> entities;

        std::shared_ptr<mn::Graphics::Window> window;
    };

    struct Application
    {
        Application(mn::Math::Vec2u size) :
            window(std::make_shared<mn::Graphics::Window>(size, "Hello"))
        {

        }

        template<typename T, typename... Args>
            requires ( std::derived_from<T, Scene> )
        void emplace_scene(Args&&... args)
        {
            scenes.emplace_back(std::make_unique<T>(window, std::forward<Args>(args)...));
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

                {
                    const auto update_block = profiler.beginBlock("SceneUpdate");
                    auto new_scene = scenes.back()->update(dt);
                    profiler.endBlock(update_block, "SceneUpdate");

                    if (new_scene.destroy)    scenes.pop_back();
                    if (new_scene.next_scene) scenes.push_back(std::move(new_scene.next_scene));
                }

                if (!scenes.size()) break;
                
                {
                    Util::ProfilerBlock render_block(profiler, "PollEvents");
                    mn::Graphics::Event event;
                    while (window->pollEvent(event))
                        scenes.back()->poll(event);
                }

                const auto frame_start = profiler.beginBlock("FrameStart");
		        auto rf = window->startFrame();
                profiler.endBlock(frame_start, "FrameStart");

                {
                    Util::ProfilerBlock render_block(profiler, "SceneRender");
                    scenes.back()->render(rf);
                }

                {
                    const std::string names[] = {
                        "SceneUpdate", "PollEvents", "FrameStart", "SceneRender", "EndFrame"
                    };
                    double total_time = 0.0;
                    for (int i = 0; i < 5; i++)
                        total_time += profiler.getBlock(names[i])->getAverageRuntime(5.0);

                    /*
                    ImGui::Begin("Application Profile");
                    
                    ImGui::BeginTable("Profiler", 3);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Block Name");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("Runtime");
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("Perc. of Iteration");
                    for (int i = 0; i < 5; i++)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s", names[i].c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%0.2f", profiler.getBlock(names[i])->getAverageRuntime(5.0));
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%0.2f%%", profiler.getBlock(names[i])->getAverageRuntime(5.0) / total_time * 100.0);
                    }
                    ImGui::EndTable();

                    ImGui::End();*/
                }

                {
                    Util::ProfilerBlock render_block(profiler, "EndFrame");
		            window->endFrame(rf);
                }
            }

            window->finishWork();
        }

    private:
        Util::Profiler profiler;

        std::shared_ptr<mn::Graphics::Window> window;
        std::vector<std::unique_ptr<Scene>> scenes;
    };
}
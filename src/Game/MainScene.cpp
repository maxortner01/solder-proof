#include "MainScene.hpp"

#include <imgui.h>

#include "../Util/DataRep.hpp"

#include "../Engine/Component.hpp"

namespace Game
{
    template<class...Fs>
    struct overloaded : Fs... {
    using Fs::operator()...;
    };

    template<class...Fs>
    overloaded(Fs...) -> overloaded<Fs...>;

    MainScene::MainScene(std::shared_ptr<mn::Graphics::Window> window) :
        Scene(window),
        renderer(world, "/shaders/main.lua")
    {   
        using namespace Engine;

        auto obj_model = res.create<Engine::Model>("dragon_model", RES_DIR "/models/xyzrgb_dragon.obj");

        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                auto e = world.entity();
                e.set(Component::Model{ .model = obj_model });
                e.set(Component::Transform{ .position = 
                    { 
                        8.f * (i - 2),
                        8.f * (j - 2), 
                        0.f
                    },
                    //.scale = { 18.6f, 18.6f, 18.6f }
                    .rotation = { mn::Math::Angle::degrees(180), mn::Math::Angle::degrees(0), mn::Math::Angle::degrees(0) },
                    .scale = { 0.04f, 0.04f, 0.04f }
                });
                /*
                e.set(Component::Transform{ .position = 
                    { 
                        ((rand() % 1000) / 500.f - 1.f) *  40.f, 
                        ((rand() % 1000) / 500.f - 1.f) *  40.f, 
                        ((rand() % 1000) / 500.f - 1.f) * -40.f - 10.f
                    },
                    //.scale = { 18.6f, 18.6f, 18.6f }
                    .scale = { 0.04f, 0.04f, 0.04f }
                });*/
                entities.push_back(e);
            }
        }

        camera = world.entity();
        camera.set(Component::Camera::make({ 1280U / 2, 720U / 2 }, mn::Math::Angle::degrees(90), { 0.1f, 500.f }));
        camera.set(Component::Transform{ .position = { 0.f, 0.f, 5.f } });

        light = world.entity();
        light.set(Component::Transform{ .position = { 0.f, 15.f, 0.f } });
        light.set(Component::Light{ .color = { 0.1f, 0.5f, 0.4f }, .intensity = 550.f });

        light2 = world.entity();
        light2.set(Component::Transform{ .position = { 0.f, -15.f, 0.f } });
        light2.set(Component::Light{ .color = { 1.f, 0.6f, 0.1f }, .intensity = 550.f });
    }

    Engine::Scene::Replace
    MainScene::update(double dt)
    {
        using namespace mn::Math;
        using namespace mn::Graphics;
        using namespace Engine;

        if (move_mouse)
            window->setMousePos((Vec2f)window->size() / 2.f);

        static std::size_t index = 0;

        Vec3f forward = [this]()
        {
            auto forward = rotation<float>(camera.get<Component::Transform>()->rotation) * Vec4f{ 0.f, 0.f, -1.f, 0.f };
            Vec3f ret;
            x(ret) = x(forward);
            y(ret) = y(forward);
            z(ret) = z(forward);

            y(ret) = 0.f;
            ret = normalized(ret);
            return ret;
        }();

        Vec3f right = [this]()
        {
            auto forward = rotation<float>(camera.get<Component::Transform>()->rotation) * Vec4f{ 1.f, 0.f, 0.f, 0.f };
            Vec3f ret;
            x(ret) = x(forward);
            y(ret) = y(forward);
            z(ret) = z(forward);

            y(ret) = 0.f;
            ret = normalized(ret);
            return ret;
        }();

        Vec3f delta;
        if (Keyboard::keyDown(Keyboard::W))
            delta += forward;
        if (Keyboard::keyDown(Keyboard::S))
            delta += forward * -1.f;

        if (Keyboard::keyDown(Keyboard::A))
            delta += right * -1.f;
        if (Keyboard::keyDown(Keyboard::D))
            delta += right;

        if (Keyboard::keyDown(Keyboard::Space))
            delta += Vec3f{  0.f, -1.f, 0.f };
        if (Keyboard::keyDown(Keyboard::LShift))
            delta += Vec3f{  0.f, 1.f, 0.f };

        if (Keyboard::keyDown(Keyboard::Escape))
            return { true };

        if (length(delta) > 0.f)
            camera.get_mut<Component::Transform>()->position += normalized( delta ) * 6.5f * dt;

        total_time += dt;

        //float _x = 1.f;
        for (const auto& e : entities)
        {
            //z(e.get_mut<Component::Transform>()->rotation) += Angle::degrees(5.0 * dt);
            y(e.get_mut<Component::Transform>()->rotation) += Angle::degrees(20.0 * dt);
            //_x += 0.01f;
        }

        //y(light2.get_mut<Component::Transform>()->position) = 40.f * sin(2.5f * total_time);
        x(light2.get_mut<Component::Transform>()->position) = 40.f * sin(-1.5f * total_time);
        x(light.get_mut<Component::Transform>()->position) = 40.f * sin(1.5f * total_time);
        //z(light.get_mut<Component::Transform>()->position) = 40.f * cos(2.5f * total_time);
        //std::cout << x(light.get_mut<Component::Transform>()->position) << "\n";

        z(camera.get_mut<Component::Transform>()->position) -= 0.5f * dt;

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

        if (!render_ui) return;

        /*
        ImGui::Begin("Rendering");
        ImGui::Checkbox("Render Wireframe", &renderer.render_wireframe)
        ImGui::End();*/

        ImGui::Begin("Entities");
        for (const auto& e : entities)
        {
            std::string text = std::string((e.name().size() ? e.name().c_str() : "Unnamed entity")) + " (id: " + std::to_string(e.raw_id()) + ")";
            if (ImGui::CollapsingHeader(text.c_str()))
            {
                if (e.has<Component::Transform>())
                {
                    double min = -mn::Math::Angle::PI.asRadians();
                    double max =  mn::Math::Angle::PI.asRadians();
                    const auto* transform = e.get<Component::Transform>();
                    ImGui::SeparatorText("Transform");
                    ImGui::SliderFloat3("Position", (float*)&transform->position, -25.f, 25.f);
                    ImGui::SliderScalarN("Rotation", ImGuiDataType_Double, (double*)&transform->rotation, 3, &min, &max);
                    ImGui::SliderFloat3("Scale", (float*)&transform->scale, 0.f, 10.f);
                }
                if (e.has<Component::Model>())
                {
                    const auto* model = e.get<Component::Model>();
                    ImGui::SeparatorText("Model");
                    ImGui::Text("Resource Name: %s", model->model.name.c_str());
                }
            }
        }
        ImGui::End();

        // Nicer way to do this
        ImGui::Begin("Resources");
        if (ImGui::CollapsingHeader("Strings"))
        {
            auto string_map = res.get_type_map<std::string>();
            for (const auto& [ name, str_ptr ] : string_map)
                if (ImGui::CollapsingHeader(name.c_str()))
                {
                    ImGui::Text("Value: %s", str_ptr->c_str());
                }
        }
        if (ImGui::CollapsingHeader("Models"))
        {
            auto model_map = res.get_type_map<Engine::Model>();
            for (const auto& [ name, model_ptr ] : model_map)
                if (ImGui::CollapsingHeader(name.c_str()))
                {
                    const auto& meshes = model_ptr->getMeshes();
                    ImGui::Text("%lu kB", Util::convert<Util::Bytes, Util::Kilobytes>(model_ptr->allocated()));
                    ImGui::Text("Contains %lu meshes", meshes.size());
                    for (const auto& mesh : meshes) ImGui::Text("  %lu vertices, %lu indices", mesh.mesh->vertexCount(), mesh.mesh->indexCount());
                    
                }
        }
        ImGui::End();
    }

    void 
    MainScene::poll(mn::Graphics::Event& e)
    {
        using namespace mn;
        using namespace mn::Graphics;

        std::visit(overloaded{
            [](const auto&) {},
            [this](const Event::WindowSize& size)
            {
                const auto* cam = camera.get<Engine::Component::Camera>();
                cam->surface->rebuildAttachment<Image::Color>       (Image::B8G8R8A8_UNORM, { size.new_width, size.new_height });
                cam->surface->rebuildAttachment<Image::DepthStencil>(Image::DF32_SU8,       { size.new_width, size.new_height });
            },
            [this](const Event::MouseMove& move)
            {
                if (move_mouse)
                {
                    auto* cam = camera.get_mut<Engine::Component::Transform>();
                    Math::y(cam->rotation) += Math::Angle::degrees(Math::x(move.delta)) * 0.2;
                    Math::x(cam->rotation) += Math::Angle::degrees(Math::y(move.delta)) * -0.2;
                }
            },
            [this](const Event::Key& key)
            {
                if (key.type == Event::ButtonType::Press && key.key == 'q')
                    render_ui = !render_ui;

                if (key.type == Event::ButtonType::Press && key.key == 'r')
                    move_mouse = !move_mouse;
            },
            [this](const Event::Quit&)
            {
                done = true;
            }
        }, e.event);
    }
}
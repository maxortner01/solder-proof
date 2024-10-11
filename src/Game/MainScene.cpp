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
        renderer(world, [this]()
            {
                auto vertex   = res.create<mn::Graphics::Shader>("vertex_shader",   RES_DIR "/shaders/vertex.glsl",   mn::Graphics::ShaderType::Vertex);
                auto fragment = res.create<mn::Graphics::Shader>("fragment_shader", RES_DIR "/shaders/fragment.glsl", mn::Graphics::ShaderType::Fragment);
                return mn::Graphics::PipelineBuilder::fromLua(RES_DIR, "/shaders/main.lua")
                    .addShader(vertex.value)
                    .addShader(fragment.value);
            }()),
        frame_index{0},
        fpses(50, 0.0),
        bunnies(world.query_builder<Bunny, Engine::Component::Transform>().build())
    {   
        using namespace Engine;

        auto obj_model = res.create<Engine::Model>("dragon_model", RES_DIR "/models/stanford-bunny.obj");

        for (int i = 0; i < 10; i++)
        {
            for (int j = 0; j < 10; j++)
            {
                auto e = createEntity();
                e.set(Component::Model{ .model = obj_model });
                e.set(Component::Transform{ .position = 
                    { 
                        8.f * (i - 5),
                        8.f * (j - 5), 
                        0.f
                    },
                    .rotation = { mn::Math::Angle::degrees(180), mn::Math::Angle::degrees(0), mn::Math::Angle::degrees(0) },
                    .scale = { 25.f, 25.f, 25.f }
                });
                e.add<Bunny>();
            }
        }

        res.create<mn::Graphics::Texture>("box_texture", RES_DIR "/textures/box.png");

        camera = createEntity("camera");
        camera.set(Component::Camera::make({ 1280U / 2, 720U / 2 }, mn::Math::Angle::degrees(90), { 0.1f, 500.f }));
        camera.set(Component::Transform{ .position = { 0.f, 0.f, 5.f } });

        light = createEntity("light");
        light.set(Component::Transform{ .position = { 0.f, 15.f, 0.f } });
        light.set(Component::Light{ .color = { 0.1f, 0.5f, 0.4f }, .intensity = 550.f });

        light2 = createEntity("light2");
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
        {
            window->setMousePos((Vec2f)window->size() / 2.f);
            window->showMouse(false);
        }
        else
            window->showMouse(true);

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

        bunnies.each(
            [&dt](Bunny, Engine::Component::Transform& transform)
            {
                y(transform.rotation) += Angle::degrees(20.0 * dt);
            });


        x(light2.get_mut<Component::Transform>()->position) = 40.f * sin(-1.5f * total_time);
        x(light.get_mut<Component::Transform>()->position) = 40.f * sin(1.5f * total_time);

        fpses[frame_index % fpses.size()] = 1.0 / dt;
        frame_index++;

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

        ImGui::Begin("Rendering");
        ImGui::Text("FPS: %u", static_cast<uint32_t>(get_fps()));
        ImGui::Checkbox("Render Wireframe", &renderer.settings.wireframe);
        ImGui::End();

        renderer.drawOverlay();
        renderOverlay();
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
                // Need to preserve the "low-res" image style
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

    double MainScene::get_fps() const
    {
        double acc = 0.0;
        for (uint32_t i = 0; i < frame_index && i < fpses.size(); i++)
            acc += fpses[i];
        return ( frame_index == 0 ? 0.0 : acc / std::min<double>(frame_index, fpses.size()) );
    }
}
#include "EditorScene.hpp"

namespace Game
{

EditorScene::EditorScene(std::shared_ptr<mn::Graphics::Window> window) :
    Scene(window), done{false}, loading_finished(std::make_shared<LoadingScene::Data>(LoadingScene::Data{ false, "" }))
{   
    target_zoom = 12.f;
}

EditorScene::~EditorScene()
{
    loading_thread->join();
}

Engine::Scene::Replace
EditorScene::update(double dt) 
{
    using namespace Engine;

    static int it = 0;
    if (it == 0)
    {
        loading_thread = std::make_unique<std::thread>([this]()
        {
            using namespace mn::Graphics;

            loading_finished->message = "Loading renderer...";
            renderer = std::make_unique<Engine::System::Renderer>(this->world);

            loading_finished->message = "Loading material systems...";
            const auto color_materials   = res.create<System::ColorMaterial>  ("color_material", res);
            const auto diffuse_materials = res.create<System::DiffuseMaterial>("diffuse_material", res);
            const auto line_materials    = res.create<System::LineMaterial>("line_material", res);

            loading_finished->message = "Loading models...";
            const auto bunny_model = res.create<Model>("bunny_model", RES_DIR "/models/stanford-bunny.obj", color_materials.value);

            loading_finished->message = "Creating editor models...";
            auto coordinate_system = res.create<Model>("coords");
            {
                auto coordinate_mesh = std::make_shared<mn::Graphics::Mesh>(
                    mn::Graphics::Mesh::fromFrame([]()
                    {
                        using Vertex = mn::Graphics::Mesh::Vertex;
                        mn::Graphics::Mesh::Frame frame;

                        for (int i = -100; i <= 100; i++)
                        {
                            frame.vertices.push_back(Vertex {
                                .position = mn::Math::Vec3f{ (float)i, 0.f, -100.f },
                                .normal = { 0.f, 1.f, 0.f },
                                .color = { 1.f, 1.f, 1.f, 0.2f }
                            });
                            frame.indices.push_back(frame.indices.size());

                            frame.vertices.push_back(Vertex {
                                .position = mn::Math::Vec3f{ (float)i, 0.f,  100.f },
                                .normal = { 0.f, 1.f, 0.f },
                                .color = { 1.f, 1.f, 1.f, 0.2f }
                            });
                            frame.indices.push_back(frame.indices.size());


                            frame.vertices.push_back(Vertex {
                                .position = mn::Math::Vec3f{ -100.f, 0.f, (float)i },
                                .normal = { 0.f, 1.f, 0.f },
                                .color = { 1.f, 1.f, 1.f, 0.2f }
                            });
                            frame.indices.push_back(frame.indices.size());

                            frame.vertices.push_back(Vertex {
                                .position = mn::Math::Vec3f{ 100.f, 0.f, (float)i },
                                .normal = { 0.f, 1.f, 0.f },
                                .color = { 1.f, 1.f, 1.f, 0.2f }
                            });
                            frame.indices.push_back(frame.indices.size());
                        }

                        return frame;
                    }()));
                auto bounded_mesh = coordinate_system.value->pushMesh(coordinate_mesh);
                bounded_mesh->material = System::Material::Instance{ .set = nullptr, .pipeline = line_materials.value->pipeline };
            }

            loading_finished->message = "Constructing entities...";
            auto e = this->world.entity();
            e.set<Component::Transform>(Component::Transform{ .position = { 0.f, 1.f, 0.f }, .scale = { 25.f, 25.f, 25.f } });
            e.set<Component::Model>(Component::Model{ .model = bunny_model, .lit = false });

            auto coord_entity = this->world.entity();
            coord_entity.set<Component::Transform>(Component::Transform{ .scale = { 1.f, 1.f, 1.f } });
            coord_entity.set<Component::Model>(Component::Model{ .model = coordinate_system, .lit = false });
            coord_entity.add<Component::DontCull>();

            const auto size = mn::Math::Vec2u{ 1280U, 720U };
            camera = this->world.entity();
            camera.set<Component::Transform>(Component::Transform{ .position = { 0.f, 0.f, 0.f } });
            camera.set<Component::Camera>(Component::Camera::make(size, mn::Math::Angle::degrees(90), { 0.01f, 500.f }));
            camera.get_mut<Component::Camera>()->type = Component::Camera::Orbit;
            camera.get_mut<Component::Camera>()->orbitDistance = 12.f;

            surface = std::make_shared<Image>(
                ImageFactory()
                    .addAttachment<mn::Graphics::Image::Color>(mn::Graphics::Image::B8G8R8A8_UNORM,  size)
                    .addAttachment<mn::Graphics::Image::DepthStencil>(mn::Graphics::Image::DF32_SU8, size)
                    .build()
            );

            loading_finished->finished = true;
        });
        it++;
        return { done, std::make_unique<LoadingScene>(window, loading_finished) };
    }

    camera.get_mut<Component::Camera>()->orbitDistance += 
        (target_zoom - camera.get_mut<Component::Camera>()->orbitDistance) * 10.2f * dt;

    return { done };
}

void
EditorScene::render(mn::Graphics::RenderFrame& rf) const 
{
    renderer->render(rf);
    rf.blit(camera.get<Engine::Component::Camera>()->surface->getColorAttachments()[0], surface->getColorAttachments()[0]);

    rf.clear({ 0.1f, 0.15f, 0.2f });
    ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Image((ImTextureID)surface->getColorAttachments()[0].imgui_ds, ImVec2(840, 480));
    
    scene_hovered = ImGui::IsItemHovered();
    window->process_imgui_events = !scene_hovered;

    ImGui::End();

    renderer->drawOverlay();
}

void 
EditorScene::poll(mn::Graphics::Event& e) 
{
    using namespace mn;
    using namespace mn::Graphics;

    std::visit(Util::overloaded{
        [](const auto&) {},
        [this](const Event::WindowSize& size)
        {
            
        },
        [this](const Event::MouseMove& move)
        {
            // if mouse down
            if (scene_hovered && mn::Graphics::Mouse::leftDown())
            {
                auto* transform = camera.get_mut<Engine::Component::Transform>();
                if (mn::Graphics::Keyboard::keyDown(mn::Graphics::Keyboard::LControl))
                {
                    const auto* camera_comp = camera.get<Engine::Component::Camera>();
                    const auto* camera_tran = camera.get<Engine::Component::Transform>();
                    const auto  viewMatrix  = camera_comp->createViewMatrix(*camera_tran);
                    const auto  invView = Math::inv(viewMatrix);
                    const auto  camera_pos_4 = invView * Math::Vec4f{ 0.f, 0.f, 0.f, 1.f };
                    const auto  right_4 = (invView * Math::Vec4f{ 1.f, 0.f, 0.f, 1.f} - camera_pos_4);
                    const auto  right = Math::normalized( Math::Vec3f{ Math::x(right_4), 0.f, Math::z(right_4) } );

                    // [ ] A better way would be to ray cast from the mouse out to the plane and get the world space displacement
                    // [ ] With ray casting implemented, we can also select objects by clicking on them
                    // [ ] Switch to the docking branch of imgui so we can start doing a real editor layout
                    
                    auto forward = Math::normalized( transform->position - Math::xyz(camera_pos_4) );
                    Math::y(forward) = 0;
                    forward = Math::normalized(forward);

                    transform->position += right   * Math::x(move.delta) * 0.08f;
                    transform->position += forward * Math::y(move.delta) * 0.08f;
                }
                else
                {
                    x(transform->rotation) += mn::Math::Angle::radians( Math::y(move.delta) ) * 0.01f;
                    y(transform->rotation) += mn::Math::Angle::radians( Math::x(move.delta) ) * -0.01f;
                }
            }
        },
        [this](const Event::Wheel& wheel)
        {
            if (scene_hovered)
            {
                target_zoom += 0.5f * wheel.amount * target_zoom;
            }
        },  
        [this](const Event::Key& key)
        {
            
        },
        [this](const Event::Quit&)
        {
            done = true;
        }
    }, e.event);
}

}
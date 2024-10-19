#include "LoadingScene.hpp"

namespace Game
{

LoadingScene::LoadingScene(std::shared_ptr<mn::Graphics::Window> window, std::shared_ptr<Data> finished_loading) :
    Scene(window), loading_finished(finished_loading)
{   }

Engine::Scene::Replace
LoadingScene::update(double dt) 
{
    return { loading_finished->finished };
}

void
LoadingScene::render(mn::Graphics::RenderFrame& rf) const 
{
    rf.clear({ 0.05f, 0.2f, 0.4f });

    ImGui::Begin("Loading...", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Loading scene: %s", loading_finished->message.c_str());
    ImGui::End();
}

void 
LoadingScene::poll(mn::Graphics::Event& e) 
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
            
        },
        [this](const Event::Key& key)
        {
            
        },
        [this](const Event::Quit&)
        {
            //done = true;
        }
    }, e.event);
}

}
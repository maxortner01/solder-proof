#include <Engine/Application.hpp>

#include <Game/MainScene.hpp>

using namespace Engine;

int main()
{
    Engine::Application app({ 1280U, 720U });
    app.emplace_scene<Game::MainScene>();
    app.run();
}
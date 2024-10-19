#include <Engine/Application.hpp>

#include <Game/EditorScene.hpp>

using namespace Engine;

int main()
{
    Engine::Application app({ 1280U, 720U });
    app.emplace_scene<Game::EditorScene>();
    app.run();
}
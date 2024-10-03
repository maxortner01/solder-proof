#include <Engine/Application.hpp>
#include <iostream>

struct MainScene : Engine::Scene
{
    double total_time = 0.0;

    Engine::Scene::Replace
    update(double dt) override
    {
        total_time += dt;
        return { ( total_time >= 1.0 ) };
    }

    void
    render() const override
    {

    }
};

int main()
{
    Engine::Application app;
    app.emplace_scene<MainScene>();
    app.run();
}
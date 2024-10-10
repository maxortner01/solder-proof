#pragma once

#include "../Engine/Application.hpp"
#include "../Engine/Model.hpp"
#include "../Engine/ResourceManager.hpp"

#include "../Engine/Systems/Renderer.hpp"

#include <set>

#include <flecs.h>

// [x] Next step: Create a mesh loader, then load in the stanford bunny to see how performance is
// [ ] Will need to consider how textures/materials are handled... Maybe later... texture descriptor should
//     be set *per* model
// [ ] Would like to be able to have a thread whose sole purpose is pushing command buffers to the queue,
//     this way resources like textures that need a command submit in order to load into the GPU can be loaded
//     in other threads (will be a little more complicated than I thought)
// [x] Need to develop Keyboard struct in midnight to get instantaneous keyboard state information (for camera motion for example)
// [x] ImGui?
// [ ] Should start tackling basic culling in the renderer
// [x] Basic resource manager
// [ ] Would like to build into the renderer the ability to look at bounding boxes and see wireframe meshes, etc.
// [ ] Need to build out a descriptor class in midnight that is separate from the pipeline, then you can pass it into
//     the pipeline builder in order to attach it to a pipeline. This way, we can create a global descriptor where we specify
//     explicitly the bindings, etc. and we can put multiple descriptors into a pipeline...
//     Moving away from the combined image sampler allows us to have many many *more* images, and move towards bindless
// [ ] Need to implement texture loading into the model class
// [ ] Use meshoptimizer to generate LOD meshes
// [ ] Consider packing the LOD meshes into the same vertex array

// [ ] Level editor: load in models, manage resources, see entities/components, create entities, etc. 

namespace Game
{
    struct MainScene : Engine::Scene
    {
        struct SceneData
        {
            mn::Math::Mat4<float> view, projection; 
        };

        flecs::entity camera, light, light2;
        flecs::world world;
        double total_time = 0.0;

        std::set<char> keys;

        Engine::ResourceManager res;
        Engine::System::Renderer renderer;

        std::vector<flecs::entity> entities;

        MainScene(std::shared_ptr<mn::Graphics::Window> window);

        Engine::Scene::Replace
        update(double dt) override;

        void
        render(mn::Graphics::RenderFrame& rf) const override;

        void 
        poll(mn::Graphics::Event& e) override;

        bool done = false;
        bool render_ui = true;
        bool move_mouse = true;
    };
}
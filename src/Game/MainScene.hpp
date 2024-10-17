#pragma once

#include "../Engine/Application.hpp"
#include "../Engine/Model.hpp"
#include "../Engine/ResourceManager.hpp"

#include "../Engine/Systems/Renderer.hpp"

#include "../Engine/Component.hpp"

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
// [x] Need to build out a descriptor class in midnight that is separate from the pipeline, then you can pass it into
//     the pipeline builder in order to attach it to a pipeline. This way, we can create a global descriptor where we specify
//     explicitly the bindings, etc. and we can put multiple descriptors into a pipeline...
//     Moving away from the combined image sampler allows us to have many many *more* images, and move towards bindless
// [ ] Need to implement texture loading into the model class
// [x] Use meshoptimizer to generate LOD meshes
// [ ] To start developing levels I need only culling and materials built in

// [ ] Level editor: load in models, manage resources, see entities/components, create entities, etc. 

namespace Game
{
    struct Bunny { };

    struct MainScene : Engine::Scene
    {
        struct SceneData
        {
            mn::Math::Mat4<float> view, projection; 
        };

        mutable Util::Profiler profiler;

        flecs::entity camera, light, light2;
        double total_time = 0.0;

        std::vector<double> fpses;

        flecs::query<Bunny, Engine::Component::Transform> bunnies;
        mutable Engine::System::Renderer renderer;

        MainScene(std::shared_ptr<mn::Graphics::Window> window);

        Engine::Scene::Replace
        update(double dt) override;

        void
        render(mn::Graphics::RenderFrame& rf) const override;

        void 
        poll(mn::Graphics::Event& e) override;

    private:
        double get_fps() const;

        std::size_t frame_index;

        bool done = false;
        bool render_ui = true;
        bool move_mouse = true;
    };
}
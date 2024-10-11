#include "Renderer.hpp"

#include "../../Util/DataRep.hpp"

#include <imgui.h>
#include <unordered_map>

namespace Engine::System
{
    void Renderer::GBuffer::rebuild(mn::Math::Vec2u size)
    {
        using namespace mn::Graphics;

        if (!position)
            position = std::make_shared<Image>(
                ImageFactory()
                    .addAttachment<Image::Color>(Image::R16G16B16A16_SFLOAT, size)
                    .build()
            );
        else
            position->rebuildAttachment<Image::Color>(Image::R16G16B16A16_SFLOAT, size);

        if (!normal)
            normal = std::make_shared<Image>(
                ImageFactory()
                    .addAttachment<Image::Color>(Image::R16G16B16A16_SFLOAT, size)
                    .build()
            );
        else
            normal->rebuildAttachment<Image::Color>(Image::R16G16B16A16_SFLOAT, size);

        if (!color)
            color = std::make_shared<Image>(
                ImageFactory()
                    .addAttachment<Image::Color>(Image::R8G8B8A8_UNORM, size)
                    .addAttachment<Image::DepthStencil>(Image::DF32_SU8, size)
                    .build()
            );
        else
        {
            color->rebuildAttachment<Image::Color>(Image::R8G8B8A8_UNORM, size);
            color->rebuildAttachment<Image::DepthStencil>(Image::DF32_SU8, size);
        }

        this->size = size;
    }

    Renderer::Renderer(flecs::world _world, const mn::Graphics::PipelineBuilder& builder) : 
        world{_world},
        model_query(_world.query_builder<const Component::Model, const Component::Transform>()
            .cached()
            .order_by<Component::Model>(
                [](flecs::entity_t e1, const Component::Model *d1, flecs::entity_t e2, const Component::Model *d2) {
                    return (d1->model.value.get() > d2->model.value.get()) - (d1->model.value.get() < d2->model.value.get());
                }
            )
            .build()),
        camera_query(_world.query_builder<const Component::Camera>()
            .cached()
            .build()),
        light_query(_world.query_builder<const Component::Light, const Component::Transform>()
            .cached()
            .build()),
        descriptor(std::make_shared<mn::Graphics::Descriptor>(
            std::move(mn::Graphics::DescriptorBuilder()
                .addBinding(mn::Graphics::Descriptor::Binding{ .type = mn::Graphics::Descriptor::Binding::Sampler, .count = 2 })
                .addVariableBinding(mn::Graphics::Descriptor::Binding::Image, 3)
                .build()))),
        pipeline(std::make_shared<mn::Graphics::Pipeline>(
            [](mn::Graphics::PipelineBuilder builder) -> mn::Graphics::Pipeline
            {
                return std::move(builder.setPushConstantObject<PushConstant>().build());
            }(builder)
        )),
        wireframe_pipeline(std::make_shared<mn::Graphics::Pipeline>(
            [](mn::Graphics::PipelineBuilder builder) -> mn::Graphics::Pipeline
            {
                return std::move(builder.setPushConstantObject<PushConstant>().setPolyMode(mn::Graphics::Polygon::Wireframe).build());
            }(builder)
        )),
        quad_mesh(std::make_shared<mn::Graphics::Mesh>(mn::Graphics::Mesh::fromFrame([]()
        {
            mn::Graphics::Mesh::Frame frame;

            frame.vertices = {
                mn::Graphics::Mesh::Vertex{ .position = { -1.f,  1.f, 0.f }, .tex_coords = { 0.f, 1.f } },
                mn::Graphics::Mesh::Vertex{ .position = {  1.f,  1.f, 0.f }, .tex_coords = { 1.f, 1.f } },
                mn::Graphics::Mesh::Vertex{ .position = {  1.f, -1.f, 0.f }, .tex_coords = { 1.f, 0.f } },
                mn::Graphics::Mesh::Vertex{ .position = { -1.f, -1.f, 0.f }, .tex_coords = { 0.f, 0.f } }
            };

            frame.indices = { 1, 0, 2, 3, 2, 0 };

            return frame;
        }()))),
        quad_pipeline(std::make_shared<mn::Graphics::Pipeline>(
            mn::Graphics::PipelineBuilder() 
                .addShader(RES_DIR "/shaders/quad.vertex.glsl",   mn::Graphics::ShaderType::Vertex)
                .addShader(RES_DIR "/shaders/quad.fragment.glsl", mn::Graphics::ShaderType::Fragment)
                .addAttachmentFormat(mn::Graphics::Image::B8G8R8A8_UNORM)
                .setDepthFormat(mn::Graphics::Image::DF32_SU8)
                .setPushConstantObject<GBufferPush>()
                .setDepthTesting(false)
                .setBackfaceCull(true)
                .addSet(descriptor)
                .build()
        ))
    {   }

    void Renderer::render(mn::Graphics::RenderFrame& rf) const
    {
        // [ ] Here we will have actual models, which might contain multiple meshes
        //     Maybe, we create a unordered_map<std::shared_ptr<Graphics::Mesh>, std::vector<Mat4<float>>>

        using namespace mn;

        struct ModelRep
        {
            std::size_t offset, count;
            std::shared_ptr<Graphics::Mesh> model;
        };

        std::vector<ModelRep> offsets;

        // Resize our GPU buffers
        scene_data.resize(camera_query.count()); 
        light_data.resize(light_query.count());

        // Create as many gbuffers as we need this frame to accomodate for all the cameras
        if (gbuffers.size() < camera_query.count())
            gbuffers.resize(camera_query.count());

        //pipeline.getSet()->setImages(0, Graphics::Backend::Sampler::Nearest, images);

        // we can keep handle the descriptor set here as well
        // go through the unique models and push the texture

        std::size_t total_instance_count = 0;
        std::unordered_map<
            std::shared_ptr<Graphics::Mesh>, 
            std::vector<InstanceData>> instance_data;

        model_query.each(
            [&](const Component::Model& model, const Component::Transform& transform)
            {
                const auto normal    = Math::rotation<float>(transform.rotation);
                const auto model_mat = Math::scale(transform.scale) * normal * Math::translation(transform.position);
                for (const auto& mesh : model.model.value->getMeshes())
                {
                    instance_data[mesh.mesh].push_back(InstanceData{
                        .model  = model_mat,
                        .normal = normal
                    });
                    total_instance_count++;
                }
            });
        
        brother_buffer.resize(total_instance_count);
        std::size_t it = 0;
        for (const auto& [ model, matrices ] : instance_data)
        {
            offsets.push_back(ModelRep{ .offset = it, .model = model });   
            std::copy(matrices.begin(), matrices.end(), &brother_buffer[it]);
            it += matrices.size();
        }

        std::vector<Math::Vec3f> view_pos;
        std::vector<std::shared_ptr<Graphics::Image>> camera_images;

        it = 0;
        camera_query.each(
            [this, &rf, &it, &camera_images, &view_pos](flecs::entity e, const Component::Camera& camera)
            {
                const auto& attach = camera.surface->getAttachment<Graphics::Image::Color>();

                if (gbuffers[it].size != attach.size)
                    gbuffers[it].rebuild(attach.size);

                rf.image_stack.push(gbuffers[it].color);
                camera_images.push_back(camera.surface);
                view_pos.push_back(e.get<Component::Transform>()->position);

                scene_data[it  ].view = ( e.has<Component::Transform>() ?
                    Math::translation(e.get<Component::Transform>()->position * -1.f) * Math::rotation<float>(e.get<Component::Transform>()->rotation * -1.0) :
                    Math::identity<4, float>()
                );
                scene_data[it++].projection = Math::perspective((float)Math::x(attach.size) / (float)Math::y(attach.size), camera.FOV, camera.near_far);
            });

        it = 0;
        light_query.each(
            [&](const Component::Light& light, const Component::Transform& transform)
            {
                light_data[it  ].position  = transform.position;
                light_data[it  ].intensity = light.intensity;
                light_data[it++].color    = light.color;
            });

        // Should probably set this per model
        std::vector<std::shared_ptr<Graphics::Backend::Sampler>> samplers;
        std::vector<std::shared_ptr<Graphics::Image>> images;
        
        for (auto& gb : gbuffers)
        {
            images.push_back(gb.color);
            images.push_back(gb.position);
            images.push_back(gb.normal);
        }
        
        {
            auto& device = Graphics::Backend::Instance::get()->getDevice();
            samplers.push_back(device->getSampler(Graphics::Backend::Sampler::Nearest));
            samplers.push_back(device->getSampler(Graphics::Backend::Sampler::Linear));
        }
        
        descriptor->update<Graphics::Descriptor::Binding::Sampler>(0, samplers);
        descriptor->update<Graphics::Descriptor::Binding::Image  >(1, images);
        
        /*
        struct CullInfo
        {
            uint32_t instance_index = -1;
            // Bounding box information
        };*/

        /*
        std::vector<CullInfo> in_instance_indices(total_instance_count);
        for (uint32_t i = 0; i < total_instance_count; i++)
            in_instance_indices[i] = CullInfo { .instance_index = i };*/

        
        //for (uint32_t i = 0; i < total_instance_count; i++)
            //instance_buffer[i] = i;

        const auto size = rf.image_stack.size();
        // For now we only support one camera
        // In the future we may need to allocate instance buffers for each camera
        // Then create yet another buffer that contains pointers to each of these buffers
        // We can then index into it with scene_index
        assert(size == 1);

        // We could record individual command buffers for each image. Then, we could
        // Render build each camera's cmd buffer out in parallel, then submit them in the end
        for (uint32_t j = 0; j < size; j++)
        {
            // For culling, we can create a list of instance indices
            //std::vector<CullInfo> out_instance_indices(total_instance_count);
            // For each camera, we take the camera info and in_instance_indices,
            // and then cull. The output of the algorithm should be the remaining instances
            // Something like this

            instance_buffer.resize(total_instance_count);
            for (int i = 0; i < total_instance_count; i++)
                instance_buffer[i] = i;

            const auto& render_data = scene_data[0];
            for (uint32_t i = 0; i < offsets.size(); i++)
            {
                int instances = ( i == offsets.size() - 1 ?
                    brother_buffer.size() - offsets[i].offset :
                    offsets[i + 1].offset - offsets[i].offset
                );

                std::size_t index = 0;
                while (index < instances)
                {
                    // Cull
                    bool within_view = true;

                    if (within_view) index++;
                    else
                    {
                        std::swap(instance_buffer[index], instance_buffer[offsets[i].offset + instances - 1]);
                        instances--;
                    }
                    //   If within view, index++
                    //   else, swap with offsets[i].offset + instances, then instances--
                }
                offsets[i].count = index;
                /*
                for (int j = 0; j < instances; j++)
                {
                    // Cull, 

                    //instance_buffer[offsets[i].offset + j] = offsets[i].offset + j;
                }*/
            }
            /*
            auto offset_it = offsets.begin();
            std::size_t current_index = 0, start_index = 0;
            for (uint32_t i = 0; i < total_instance_count; i++)
            {
                if (i > 0 && i == offset_it->offset) 
                {
                    offset_it->offset = start_index;
                    offset_it++;

                }
                instance_buffer[i] = i;
            }*/

            std::vector<std::shared_ptr<Graphics::Image>> images = { gbuffers[j].position, gbuffers[j].normal };
            for (const auto& image : images)
            {
                rf.image_stack.push(image);
                rf.clear({ 0.f, 0.f, 0.f }, 0.f);
                rf.image_stack.pop();
            }
            rf.clear({ 0.f, 0.f, 0.f }, 0.f);
            rf.startRender(images);
            for (std::size_t i = 0; i < offsets.size(); i++)
            {
                if (!offsets[i].count) continue;
                // Calculate count
                const auto instances = ( i == offsets.size() - 1 ?
                    brother_buffer.size() - offsets[i].offset :
                    offsets[i + 1].offset - offsets[i].offset
                );

                rf.setPushConstant(*(settings.wireframe ? wireframe_pipeline : pipeline), PushConstant {
                    .scene_index      = j, 
                    .offset           = static_cast<uint32_t>(offsets[i].offset),
                    .light_count      = static_cast<uint32_t>(light_data.size()),
                    .lights           = light_data.getAddress(),
                    .scene_data       = scene_data.getAddress(),
                    .instance_indices = instance_buffer.getAddress(),
                    .models           = reinterpret_cast<void*>( reinterpret_cast<std::size_t>(brother_buffer.getAddress()) + offsets[i].offset )
                });

                // Would like to extract the *binding* that happens here and do it one for all meshes, instead of for each mesh
                rf.draw((settings.wireframe ? wireframe_pipeline : pipeline), offsets[i].model, offsets[i].count); //instances);
            }

            rf.endRender();
            rf.image_stack.pop();

            rf.image_stack.push(camera_images[j]);
            rf.clear({ 0.f, 0.f, 0.f });
            rf.startRender();

            rf.setPushConstant(*quad_pipeline, GBufferPush {
                .light_count      = static_cast<uint32_t>(light_data.size()),
                .lights           = light_data.getAddress(),
                .view_pos = view_pos[j]
            });

            rf.draw(quad_pipeline, quad_mesh);
            rf.endRender();
            rf.image_stack.pop();
        }
    }
    
    void Renderer::drawOverlay() const
    {
        ImGui::Begin("Renderer");

        ImGui::SeparatorText("Memory Info");
        ImGui::Text("Cameras: %i", camera_query.count());
        ImGui::Text("Lights:  %i", light_query.count());
        ImGui::Text("Models:  %i", model_query.count());
        ImGui::Text("Total GPU Memory: %lu kB", 
            Util::convert<Util::Bytes, Util::Kilobytes>(brother_buffer.allocated() + instance_buffer.allocated() + scene_data.allocated() + light_data.allocated())
        );

        ImGui::SeparatorText((std::stringstream() << "G-Buffers: " << gbuffers.size()).str().c_str());
        for (std::size_t i = 0; i < gbuffers.size(); i++)
        {
            if (ImGui::TreeNode((std::stringstream() << "G-Buffer " << i).str().c_str()))
            {
                // Display gbuffer
                const auto& gbuffer = gbuffers[i];
                if (!gbuffer.position)
                    ImGui::Text("No position buffer");
                else
                {
                    const auto& attach = gbuffer.position->getAttachment<mn::Graphics::Image::Color>();
                    auto rf = (float)mn::Math::x(attach.size) / (float)mn::Math::y(attach.size);
                    ImGui::Image((ImTextureID)attach.imgui_ds, ImVec2(160 * rf, 160));
                    ImGui::Text("Position Image, Size (%u, %u), Handle %p", mn::Math::x(attach.size), mn::Math::y(attach.size), attach.handle);
                }

                if (!gbuffer.normal)
                    ImGui::Text("No normal buffer");
                else
                {
                    const auto& attach = gbuffer.normal->getAttachment<mn::Graphics::Image::Color>();
                    auto rf = (float)mn::Math::x(attach.size) / (float)mn::Math::y(attach.size);
                    ImGui::Image((ImTextureID)attach.imgui_ds, ImVec2(160 * rf, 160));
                    ImGui::Text("Normal Image, Size (%u, %u), Handle %p", mn::Math::x(attach.size), mn::Math::y(attach.size), attach.handle);
                }

                if (!gbuffer.color)
                    ImGui::Text("No color buffer");
                else
                {
                    const auto& attach = gbuffer.color->getAttachment<mn::Graphics::Image::Color>();
                    auto rf = (float)mn::Math::x(attach.size) / (float)mn::Math::y(attach.size);
                    ImGui::Image((ImTextureID)attach.imgui_ds, ImVec2(160 * rf, 160));
                    ImGui::Text("Color Image, Size (%u, %u), Handle %p", mn::Math::x(attach.size), mn::Math::y(attach.size), attach.handle);
                }

                ImGui::TreePop();
            }
        }

        ImGui::End();
    }
}
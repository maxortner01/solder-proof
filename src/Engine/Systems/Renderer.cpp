#include "Renderer.hpp"

#include "../../Util/DataRep.hpp"

#include <imgui.h>
#include <unordered_map>

namespace Engine::System
{
    void Renderer::GBuffer::rebuild(mn::Math::Vec2u size)
    {
        using namespace mn::Graphics;

        if (!gbuffer)
        {
            gbuffer = std::make_shared<Image>(
                ImageFactory()
                    .addAttachment<Image::Color>(Image::R8G8B8A8_UNORM, size)
                    .addAttachment<Image::Color>(Image::R16G16B16A16_SFLOAT, size)
                    .addAttachment<Image::Color>(Image::R16G16B16A16_SFLOAT, size)
                    .addAttachment<Image::DepthStencil>(Image::DF32_SU8, size)
                    .build()
            );
        }
        else
        {
            auto& attachments = gbuffer->getColorAttachments();
            attachments[0].rebuild<Image::Color>(Image::R8G8B8A8_UNORM, size);
            attachments[1].rebuild<Image::Color>(Image::R16G16B16A16_SFLOAT, size);
            attachments[2].rebuild<Image::Color>(Image::R16G16B16A16_SFLOAT, size);
            gbuffer->getDepthAttachment().rebuild<Image::DepthStencil>(Image::DF32_SU8, size);
        }

        if (!hdr_surface)
            hdr_surface = std::make_shared<Image>(
                ImageFactory()
                    .addAttachment<Image::Color>(Image::R16G16B16A16_SFLOAT, size)
                    .build()
            );
        else
            hdr_surface->getColorAttachments()[0].rebuild<Image::Color>(Image::R16G16B16A16_SFLOAT, size);

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
                .addVariableBinding(mn::Graphics::Descriptor::Binding::Image, 4)
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
                return std::move(builder.setPushConstantObject<PushConstant>()
                    .setBackfaceCull(false)
                    .setPolyMode(mn::Graphics::Polygon::Wireframe)
                    .build());
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
        cube_model(std::make_shared<Engine::Model>(RES_DIR "/models/cube.obj")),
        profiler(std::make_shared<Util::Profiler>())
    {   
        mn::Graphics::PipelineBuilder quad_builder;
        quad_builder.addShader(RES_DIR "/shaders/quad.vertex.glsl",   mn::Graphics::ShaderType::Vertex);
        quad_builder.setDepthTesting(false);
        quad_builder.setBackfaceCull(true);
        quad_builder.addSet(descriptor);

        quad_pipeline = std::make_shared<mn::Graphics::Pipeline>(
            [](mn::Graphics::PipelineBuilder builder)
            {
                return builder
                    .addShader(RES_DIR "/shaders/quad.fragment.glsl", mn::Graphics::ShaderType::Fragment)
                    .addAttachmentFormat(mn::Graphics::Image::R16G16B16A16_SFLOAT)
                    .setPushConstantObject<GBufferPush>()
                    .build();
            }(quad_builder)
        );

        hdr_pipeline = std::make_shared<mn::Graphics::Pipeline>(
            [](mn::Graphics::PipelineBuilder builder)
            {
                return builder
                    .addShader(RES_DIR "/shaders/hdr.fragment.glsl", mn::Graphics::ShaderType::Fragment)
                    .addAttachmentFormat(mn::Graphics::Image::B8G8R8A8_UNORM)
                    .setDepthFormat(mn::Graphics::Image::DF32_SU8)
                    .setPushConstantObject<HDRPush>()
                    .build();
            }(quad_builder)
        );
    }

    void Renderer::render(mn::Graphics::RenderFrame& rf) const
    {
        // [x] Here we will have actual models, which might contain multiple meshes
        //     Maybe, we create a unordered_map<std::shared_ptr<Graphics::Mesh>, std::vector<Mat4<float>>>
        // [ ] Next, we want to visualize the bounding boxes. So maybe we also load a box mesh into the renderer
        //     and have an option in the settings for drawing it. Drawing it is just as simple as adding a draw call with
        //     the wireframe pipeline with the box mesh right after we draw the model instances (same arguments and everything)

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

        auto flecs_block = profiler->beginBlock("FlecsBlock");

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
                const auto& attach = camera.surface->getColorAttachments()[0];

                if (gbuffers[it].size != attach.size)
                    gbuffers[it].rebuild(attach.size);

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

        profiler->endBlock(flecs_block, "FlecsBlock");

        const auto desc_write = profiler->beginBlock("DescWrite");

        // Should probably set this per model
        std::vector<std::shared_ptr<Graphics::Backend::Sampler>> samplers;
        std::vector<std::shared_ptr<Graphics::Image>> images;
        
        for (auto& gb : gbuffers)
        {
            images.push_back(gb.gbuffer);
            images.push_back(gb.hdr_surface);
        }
        
        {
            auto& device = Graphics::Backend::Instance::get()->getDevice();
            samplers.push_back(device->getSampler(Graphics::Backend::Sampler::Nearest));
            samplers.push_back(device->getSampler(Graphics::Backend::Sampler::Linear));
        }
        
        descriptor->update<Graphics::Descriptor::Binding::Sampler>(0, samplers);
        descriptor->update<Graphics::Descriptor::Binding::Image  >(1, images);

        profiler->endBlock(desc_write, "DescWrite");

        // For now we only support one camera
        // In the future we may need to allocate instance buffers for each camera
        // Then create yet another buffer that contains pointers to each of these buffers
        // We can then index into it with scene_index
        assert(camera_query.count() == 1);

        const auto cmd_record = profiler->beginBlock("CmdRecord");

        for (uint32_t j = 0; j < camera_query.count(); j++)
        {
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
                    // Cull here
                    bool within_view = true;

                    //   If within view, index++
                    if (within_view) index++;
                    //   else, swap with offsets[i].offset + instances, then instances--
                    else
                    {
                        std::swap(instance_buffer[index], instance_buffer[offsets[i].offset + instances - 1]);
                        instances--;
                    }
                }
                offsets[i].count = index;
            }

            rf.clear({ 0.f, 0.f, 0.f }, 0.f);
            rf.clear({ 0.f, 0.f, 0.f }, 0.f, gbuffers[j].gbuffer);
           
            rf.startRender(gbuffers[j].gbuffer);

            const auto use_pipeline = (settings.wireframe ? wireframe_pipeline : pipeline);
            rf.bind(use_pipeline);
            
            for (std::size_t i = 0; i < offsets.size(); i++)
            {
                if (!offsets[i].count) continue;

                rf.setPushConstant(*use_pipeline, PushConstant {
                    .scene_index      = j, 
                    .offset           = static_cast<uint32_t>(offsets[i].offset),
                    .light_count      = static_cast<uint32_t>(light_data.size()),
                    .lights           = light_data.getAddress(),
                    .scene_data       = scene_data.getAddress(),
                    .instance_indices = instance_buffer.getAddress(),
                    .models           = brother_buffer.getAddress(),
                    .enable_lighting  = 1
                });

                // Would like to extract the *binding* that happens here and do it one for all meshes, instead of for each mesh
                rf.draw(offsets[i].model, offsets[i].count); //instances);
                /*
                rf.bind(wireframe_pipeline);

                rf.setPushConstant(*use_pipeline, PushConstant {
                    .scene_index      = j, 
                    .offset           = static_cast<uint32_t>(offsets[i].offset),
                    .light_count      = static_cast<uint32_t>(light_data.size()),
                    .lights           = light_data.getAddress(),
                    .scene_data       = scene_data.getAddress(),
                    .instance_indices = instance_buffer.getAddress(),
                    .models           = brother_buffer.getAddress(),
                    .enable_lighting  = 0
                });

                rf.draw(cube_model->getMeshes()[0].mesh, offsets[i].count);*/
            }

            // If draw_bounding_box
            // We need to have a copy of the brother_buffer here and calculate the correct model transform 
            // to make the cube fit the min/max of the aabb box. Do push constant and everything exactly the same 

            rf.endRender();

            rf.clear({ 0.f, 0.f, 0.f }, 0.f, gbuffers[j].hdr_surface);
            rf.startRender(gbuffers[j].hdr_surface);

            rf.setPushConstant(*quad_pipeline, GBufferPush {
                .light_count      = static_cast<uint32_t>(light_data.size()),
                .lights           = light_data.getAddress(),
                .view_pos = view_pos[j]
            });

            // Take our scene and render it into the HDR surface doing the normal lighting
            // calculations
            rf.draw(quad_pipeline, quad_mesh);

            rf.endRender();

            // TODO: Here run a compute shader that goes through all the pixels and atomicAdd's the
            // brightness / (total pixel count) of each one to a storage buffer within the Gbuffer class
            // We put a barrier here so the next drawing commands wait on the execution of the compute shader
            // Once this is completed, *then* we run the hdr shader, which can read from this buffer to get
            // the L_avg of the scene, which it can then use to calculate the exposure needed to correctly render
            // the scene

            // Then we take this image and render onto the camera surface with the HDR shader
            // The HDR shader should start out doing basic tone mapping
            // Then, we can integrate further stages like automatic exposure which we can do by extracting
            // the luminance histogram data using a compute shader
            // https://bruop.github.io/exposure/

            rf.clear({ 0.f, 0.f, 0.f }, 0.f, camera_images[j]);
            rf.startRender(camera_images[j]);

            rf.setPushConstant(*hdr_pipeline, HDRPush {
                .index = j,
                .exposure = 0.02f
            });

            rf.draw(hdr_pipeline, quad_mesh);

            rf.endRender();
        }

        profiler->endBlock(cmd_record, "CmdRecord");
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

        ImGui::SeparatorText("Execution Timing (over last 5 seconds)");

        std::string names[] = { "FlecsBlock", "DescWrite", "CmdRecord" };
        double total_runtime = 0.0;

        for (int i = 0; i < 3; i++)
        {
            const auto time = profiler->getBlock(names[i])->getAverageRuntime(5.0);
            total_runtime += time;
        }

        ImGui::BeginTable("TimeTable", 3);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Block");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("Runtime");
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("Perc. of Iteration");

        for (int i = 0; i < 3; i++)
        {
            const auto time = profiler->getBlock(names[i])->getAverageRuntime(5.0);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", names[i].c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.2fms", time);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f%%", time / total_runtime);
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Total Runtime:");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%.2fms,", total_runtime);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("~%.2f FPS", 1000.0 / total_runtime);

        ImGui::EndTable();

        ImGui::SeparatorText((std::stringstream() << "G-Buffers: " << gbuffers.size()).str().c_str());
        for (std::size_t i = 0; i < gbuffers.size(); i++)
        {
            if (ImGui::TreeNode((std::stringstream() << "G-Buffer " << i).str().c_str()))
            {
                // Display gbuffer
                const auto& gbuffer = gbuffers[i];
                const auto& attachments = gbuffer.gbuffer->getColorAttachments();
                for (const auto& a : attachments)
                {
                    auto rf = (float)mn::Math::x(a.size) / (float)mn::Math::y(a.size);
                    ImGui::Image((ImTextureID)a.imgui_ds, ImVec2(160 * rf, 160));
                    ImGui::Text("Size (%u, %u), Handle %p", mn::Math::x(a.size), mn::Math::y(a.size), a.handle);
                }

                ImGui::TreePop();
            }
        }

        ImGui::End();
    }
}
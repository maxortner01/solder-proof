#include "Renderer.hpp"

#include "../../Util/DataRep.hpp"

#include "Material.hpp"

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

    Renderer::Renderer(flecs::world _world) : 
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
        quad_mesh(std::make_shared<mn::Graphics::Mesh>(mn::Graphics::Mesh::fromFrame([]()
        {
            mn::Graphics::Mesh::Frame frame;

            frame.vertices = {
                mn::Graphics::Mesh::Vertex{ .position = { -1.f,  1.f, 0.f }, .tex_coords = { 0.f, 0.f } },
                mn::Graphics::Mesh::Vertex{ .position = {  1.f,  1.f, 0.f }, .tex_coords = { 1.f, 0.f } },
                mn::Graphics::Mesh::Vertex{ .position = {  1.f, -1.f, 0.f }, .tex_coords = { 1.f, 1.f } },
                mn::Graphics::Mesh::Vertex{ .position = { -1.f, -1.f, 0.f }, .tex_coords = { 0.f, 1.f } }
            };

            frame.indices = { 1, 0, 2, 3, 2, 0 };

            return frame;
        }()))),
        profiler(std::make_shared<Util::Profiler>())
    {   
        using namespace mn::Graphics;

        gbuffer_descriptor_layout = std::make_shared<Descriptor::Layout>([]()
        {
            DescriptorLayoutBuilder layout_builder;
            layout_builder.addBinding(Descriptor::Layout::Binding{ .type = Descriptor::Layout::Binding::Sampler, .count = 2 });
            layout_builder.addVariableBinding(Descriptor::Layout::Binding::Image, 4);
            return layout_builder.build();
        }());

        auto descriptor_pool = Descriptor::Pool::make();
        gbuffer_descriptor = descriptor_pool->allocateDescriptor(gbuffer_descriptor_layout);

        mn::Graphics::PipelineBuilder quad_builder;
        quad_builder.addShader(RES_DIR "/shaders/quad.vertex.glsl", mn::Graphics::ShaderType::Vertex);
        quad_builder.setDepthTesting(false);
        quad_builder.setBackfaceCull(true);
        quad_builder.addDescriptorLayout(gbuffer_descriptor_layout);

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
        
        // We have LOD beginning to be integrated... This is the bottleneck currently (then frustum culling)
        // The way this is all set up is not going to work, though
        // Instead of passing along meshes, we need to pass along the unique combinations of a mesh and possibly
        // an outside index buffer
        // One way to do this is to have
        // std::unordered_map<std::shared_ptr<TypeBuffer<Vertex>>, std::vector<std::shared_ptr<TypeBuffer<uint32_t>>>>
        // When we iterate through the models, we can match up a vertex buffer with its corresponding index buffer
        // Then, in the render, we go through the vertex buffers like we go through models and bind, but within that
        // go through its corresonding list of index buffers and bind index buffers and draw
        // Essentially, it is sorted by mesh right now. We want to instead sort by vertex array, then within these sublists
        // sort by the index array

        using namespace mn;
        using namespace mn::Graphics;

        struct ModelRep
        {
            std::size_t offset, count;
            std::shared_ptr<TypeBuffer<Mesh::Vertex>> vertex;
            std::shared_ptr<TypeBuffer<uint32_t>> index;
            std::size_t index_offset, index_count;
            System::Material::Instance material;
            BoundingBox aabb;
            bool lit;
        };

        std::vector<ModelRep> offsets;

        // Resize our GPU buffers
        scene_data.resize(camera_query.count()); 
        light_data.resize(light_query.count());

        // Create as many gbuffers as we need this frame to accomodate for all the cameras
        if (gbuffers.size() < camera_query.count())
            gbuffers.resize(camera_query.count());

        // we can keep handle the descriptor set here as well
        // go through the unique models and push the texture

        total_instance_count = 0;
        std::unordered_map<
            std::shared_ptr<Model::BoundedMesh>, std::vector<InstanceData>> instance_data;

        auto flecs_block = profiler->beginBlock("FlecsBlock");

        struct CameraRep
        {
            Component::Transform transform;
            Component::Camera camera;
        };

        std::vector<CameraRep> cameras;
        std::vector<std::shared_ptr<Graphics::Image>> camera_images;

        auto camera_query_block = profiler->beginBlock("CameraQuery");
        std::size_t it = 0;
        camera_query.each(
            [this, &rf, &it, &camera_images, &cameras](flecs::entity e, const Component::Camera& camera)
            {
                const auto& attach = camera.surface->getColorAttachments()[0];

                if (gbuffers[it].size != attach.size)
                    gbuffers[it].rebuild(attach.size);

                camera_images.push_back(camera.surface);
                cameras.push_back(CameraRep{
                    .transform = *e.get<Component::Transform>(),
                    .camera    = camera
                });

                scene_data[it  ].view = camera.createViewMatrix(*e.get<Component::Transform>());
                scene_data[it++].projection = Math::perspective((float)Math::x(attach.size) / (float)Math::y(attach.size), camera.FOV, camera.near_far);
            });
        profiler->endBlock(camera_query_block, "CameraQuery");

        assert(cameras.size() == 1);

        const auto model_query_block = profiler->beginBlock("ModelQuery"); 
        model_query.each(
            [&](flecs::entity e, const Component::Model& model, const Component::Transform& transform)
            {
                if (e.has<Component::Hidden>()) return;

                const auto normal    = Math::rotation<float>(transform.rotation);
                const auto model_mat = Math::scale(transform.scale) * normal * Math::translation(transform.position);
                for (const auto& mesh : model.model.value->getMeshes())
                {
                    if (e.has<Component::DontCull>() || !cull(mesh->aabb, model_mat, cameras[0].transform, cameras[0].camera))
                    {
                        instance_data[mesh].push_back(InstanceData{
                            .model  = model_mat,
                            .normal = normal,
                            .lit    = model.lit
                        });
                        total_instance_count++;
                    }
                }
            });
        profiler->endBlock(model_query_block, "ModelQuery"); 
        
        const auto instance_copy = profiler->beginBlock("InstanceCopy");

        if (brother_buffer.size() < total_instance_count)
            brother_buffer.resize(total_instance_count);

        it = 0;
        for (auto& [ model, matrices ] : instance_data)
        {
            if (!matrices.size()) continue;

            // Here we sort the matrices based off distance from camera 
            std::vector<std::pair<InstanceData, float>> distances;
            for (auto& matrix : matrices)
            {
                const auto d1 = matrix.model * Math::Vec4f{0.f, 0.f, 0.f, 1.f};
                const auto distance = Math::length(Math::Vec3f{Math::x(d1), Math::y(d1), Math::z(d1)} - cameras[0].transform.position);
                distances.push_back({ matrix, distance });
            }

            std::sort(distances.begin(), distances.end(), [](const auto& mat1, const auto& mat2)
            { return mat1.second < mat2.second; });
            
            for (std::size_t i = 0; i < distances.size(); i++)
                brother_buffer[it + i] = distances[i].first;

            const std::pair<float, std::optional<float>> lod_ranges[] = {
                { 35.f, std::nullopt },
                { 30.f, 35.f },
                { 25.f, 30.f },
                { 20.f, 25.f },
                { 10.f, 20.f },
                {  0.f, 10.f }
            };

            const auto get_range = [&lod_ranges](float distance)
            {
                for (std::size_t i = 0; i < 6; i++)
                {
                    if (lod_ranges[i].second)
                    {
                        if (distance < *lod_ranges[i].second && distance >= lod_ranges[i].first)
                            return i;
                    }
                    else
                    {
                        if (distance >= lod_ranges[i].first)
                            return i;
                    }
                }
                return std::size_t(0);
            };

            std::optional<std::size_t> current_index;
            for (int i = 0; i < distances.size(); i++)   
            {
                const auto index = get_range(distances[i].second);
                if (!current_index || (current_index && *current_index != index))
                {
                    if (index < model->lods.lod_offsets.size())
                    {
                        offsets.push_back(ModelRep{ 
                            .offset = it + i, 
                            .vertex = model->mesh->vertex, 
                            .index = model->lods.lod, 
                            .index_offset = model->lods.lod_offsets[index].offset, 
                            .index_count = model->lods.lod_offsets[index].count,
                            .material = model->material,
                            .aabb = model->aabb
                        });
                    }
                    else
                    {
                        offsets.push_back(ModelRep{ 
                            .offset = it + i, 
                            .vertex = model->mesh->vertex, 
                            .index = model->mesh->index, 
                            .index_offset = 0, 
                            .index_count = model->mesh->index->size(),
                            .material = model->material,
                            .aabb = model->aabb
                        });
                    }

                    current_index = index;
                }
            }

            it += matrices.size();

            // Then we take the LOD cutoffs and push the offsets to partition this sub-field
            // modulating the index_offset and index_count variables
        }
        profiler->endBlock(instance_copy, "InstanceCopy");

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
        
        gbuffer_descriptor->update<Graphics::Descriptor::Layout::Binding::Sampler>(0, samplers);
        gbuffer_descriptor->update<Graphics::Descriptor::Layout::Binding::Image  >(1, images);

        profiler->endBlock(desc_write, "DescWrite");

        // For now we only support one camera
        // In the future we may need to allocate instance buffers for each camera
        // Then create yet another buffer that contains pointers to each of these buffers
        // We can then index into it with scene_index
        assert(camera_query.count() == 1);

        const auto cmd_record = profiler->beginBlock("CmdRecord");

        for (uint32_t j = 0; j < camera_query.count(); j++)
        {
            if (instance_buffer.size() < total_instance_count)
                instance_buffer.resize(total_instance_count);

            for (int i = 0; i < total_instance_count; i++)
                instance_buffer[i] = i;

            for (uint32_t i = 0; i < offsets.size(); i++)
            {
                int instances = ( i == offsets.size() - 1 ?
                    brother_buffer.size() - offsets[i].offset :
                    offsets[i + 1].offset - offsets[i].offset
                );

                offsets[i].count = instances;

                /*
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
                offsets[i].count = index;*/
            }

            rf.clear({ 0.f, 0.f, 0.f }, 0.f);
            rf.clear({ 0.f, 0.f, 0.f }, 0.f, gbuffers[j].gbuffer);
           
            rf.startRender(gbuffers[j].gbuffer);

            Material::Instance current_material;
            
            for (std::size_t i = 0; i < offsets.size(); i++)
            {
                if (!offsets[i].count) continue;

                rf.setPushConstant(*offsets[i].material.pipeline, PushConstant {
                    .scene_index        = j, 
                    .offset             = static_cast<uint32_t>(offsets[i].offset),
                    .light_count        = static_cast<uint32_t>(light_data.size()),
                    .lights             = light_data.getAddress(),
                    .scene_data         = scene_data.getAddress(),
                    .instance_indices   = instance_buffer.getAddress(),
                    .models             = brother_buffer.getAddress(),
                    .enable_lighting    = 1
                });

                // Would like to extract the *binding* that happens here and do it one for all meshes, instead of for each mesh
                // The models are sorted by
                // VERTEX 0
                // INDEX 0
                // INDEX 1
                // INDEX 2
                // VERTEX 1
                // INDEX 0
                // ...
                // So we only need to bind the vertex buffer if it has changed

                if (offsets[i].material.pipeline != current_material.pipeline)
                {
                    if (offsets[i].material.pipeline)
                        rf.bind(offsets[i].material.pipeline);
                    current_material.pipeline = offsets[i].material.pipeline;
                }

                if (offsets[i].material.set != current_material.set)
                {
                    if (offsets[i].material.set)
                        rf.bind(0, offsets[i].material.pipeline, offsets[i].material.set);
                    current_material.set = offsets[i].material.set;   
                }

                rf.drawIndexed(
                    offsets[i].vertex, 
                    offsets[i].index, 
                    offsets[i].count, 
                    offsets[i].index_offset, 
                    offsets[i].index_count);
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
                .view_pos         = cameras[j].transform.position
            });

            // Take our scene and render it into the HDR surface doing the normal lighting
            // calculations
            rf.bind(0, quad_pipeline, gbuffer_descriptor);
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

            rf.bind(0, hdr_pipeline, gbuffer_descriptor);
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
        ImGui::Text("Render Instance Count: %lu", total_instance_count);
        ImGui::Text("Total GPU Memory: %lu kB", 
            Util::convert<Util::Bytes, Util::Kilobytes>(brother_buffer.allocated() + instance_buffer.allocated() + scene_data.allocated() + light_data.allocated())
        );

        ImGui::SeparatorText("Execution Timing (over last 5 seconds)");

        std::string names[] = { "FlecsBlock", "InstanceCopy", "ModelQuery", "CameraQuery", "DescWrite", "CmdRecord" };
        double total_runtime = 
            profiler->getBlock("FlecsBlock")->getAverageRuntime(5.0) +
            profiler->getBlock("DescWrite")->getAverageRuntime(5.0) + 
            profiler->getBlock("CmdRecord")->getAverageRuntime(5.0);

        ImGui::BeginTable("TimeTable", 3);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Block");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("Runtime");
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("Perc. of Iteration");

        for (int i = 0; i < 6; i++)
        {
            const auto time = profiler->getBlock(names[i])->getAverageRuntime(5.0);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", names[i].c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.2fms", time);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f%%", time / total_runtime * 100.0);
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
                    ImGui::Image((ImTextureID)a.imgui_ds, ImVec2(80 * rf, 80));
                    ImGui::SameLine();
                }

                ImGui::TreePop();
            }
        }

        ImGui::End();
    }

    // Me and my buddy ChatGPT wrote this function
    bool Renderer::cull(const BoundingBox& aabb, const mn::Math::Mat4<float>& model, const Component::Transform& transform, const Component::Camera& camera) const
    {
        //return false;

        using namespace mn;

        const auto viewMatrix = camera.createViewMatrix(transform);
        const auto invView = Math::inv(viewMatrix);
        const auto camera_pos_4 = invView * Math::Vec4f{ 0.f, 0.f, 0.f, 1.f };

        auto position = Math::xyz(camera_pos_4);

        // Step 1: Transform AABB to world space
        BoundingBox worldAABB;
        // Compute the 8 corners of the AABB in local space
        std::array<Math::Vec4f, 8> corners = {
            Math::Vec4f({ Math::x(aabb.min), Math::y(aabb.min), Math::z(aabb.min), 1.f }) * 0.9f,
            Math::Vec4f({ Math::x(aabb.max), Math::y(aabb.min), Math::z(aabb.min), 1.f }) * 0.9f,
            Math::Vec4f({ Math::x(aabb.min), Math::y(aabb.max), Math::z(aabb.min), 1.f }) * 0.9f,
            Math::Vec4f({ Math::x(aabb.max), Math::y(aabb.max), Math::z(aabb.min), 1.f }) * 0.9f,
            Math::Vec4f({ Math::x(aabb.min), Math::y(aabb.min), Math::z(aabb.max), 1.f }) * 0.9f,
            Math::Vec4f({ Math::x(aabb.max), Math::y(aabb.min), Math::z(aabb.max), 1.f }) * 0.9f,
            Math::Vec4f({ Math::x(aabb.min), Math::y(aabb.max), Math::z(aabb.max), 1.f }) * 0.9f,
            Math::Vec4f({ Math::x(aabb.max), Math::y(aabb.max), Math::z(aabb.max), 1.f }) * 0.9f,
        };
        
        // Transform corners by model matrix to get world-space AABB
        for (auto& corner : corners)
            corner = model * corner; // Assuming overloaded * for Mat4 * Vec3f

        // Compute new world AABB from transformed corners
        worldAABB.min = worldAABB.max = Math::xyz(corners[0]);
        for (const auto& corner : corners)
        {
            worldAABB.min = Math::min(worldAABB.min, Math::xyz(corner));
            worldAABB.max = Math::max(worldAABB.max, Math::xyz(corner));
        }

        if (Math::x(position) >= Math::x(worldAABB.min) && Math::x(position) <= Math::x(worldAABB.max) &&
            Math::y(position) >= Math::y(worldAABB.min) && Math::y(position) <= Math::y(worldAABB.max) &&
            Math::z(position) >= Math::z(worldAABB.min) && Math::z(position) <= Math::z(worldAABB.max))
        {
            // Camera is inside the bounding box, don't cull the object
            return false;
        }

        // Step 2: Construct the frustum planes from the camera
        // Get camera forward, right, and up vectors based on the rotation
        const auto right_4 = (invView * Math::Vec4f{ 1.f, 0.f, 0.f, 1.f} - camera_pos_4);
        const auto right = Math::normalized( Math::xyz(right_4) );

        const auto up_4 = (invView * Math::Vec4f{ 0.f, 1.f, 0.f, 1.f} - camera_pos_4);
        const auto up = Math::normalized( Math::xyz(up_4) );

        const auto forward_4 = (invView * Math::Vec4f{ 0.f, 0.f, -1.f, 1.f} - camera_pos_4);
        const auto forward = Math::normalized( Math::xyz(forward_4) );

        //auto forward = Math::rotateQuaternion(transform.rotation * -1.f, Math::Vec3f({ 0.0f, 0.0f, -1.0f })); // Assuming camera looks down -Z
        //auto right   = Math::rotateQuaternion(transform.rotation * -1.f, Math::Vec3f({ 1.0f, 0.0f,  0.0f }));
        //auto up      = Math::rotateQuaternion(transform.rotation * -1.f, Math::Vec3f({ 0.0f, 1.0f,  0.0f }));

        // Calculate near and far distances
        float nearDist = Math::x(camera.near_far);
        float farDist  = Math::y(camera.near_far);

        if (Math::length(position - worldAABB.min) <= nearDist + 1.f || 
            Math::length(position - worldAABB.max) <= nearDist + 1.f)
        {
            return false; // Don't cull if the object is very close to the near plane
        }

        // Calculate half dimensions of near and far planes based on FOV and aspect ratio

        // TODO: For some reason changing the FOV doesn't change the culling...
        float aspectRatio = (float)Math::x(camera.surface->getColorAttachments()[0].size) / (float)Math::y(camera.surface->getColorAttachments()[0].size); // Assuming 16:9 aspect ratio, adjust if needed
        float nearHeight = 2.0f * tanf(camera.FOV.asRadians() / 2.0f) * nearDist;
        float nearWidth = nearHeight * aspectRatio;
        float farHeight = 2.0f * tanf(camera.FOV.asRadians() / 2.0f) * farDist;
        float farWidth = farHeight * aspectRatio;

        // Calculate centers of near and far planes
        Math::Vec3f nearCenter = position + forward * nearDist;
        Math::Vec3f farCenter  = position + forward * farDist;
        // Step 3: Define the frustum planes (normal points inward)
        // Step 3: Define the frustum planes (normal points inward)
        std::array<Math::Vec4f, 6> frustumPlanes;

        // Near plane
        frustumPlanes[0] = Math::Vec4f({ Math::x(forward), Math::y(forward), Math::z(forward), -Math::inner(forward, nearCenter) });

        // Far plane
        frustumPlanes[1] = Math::Vec4f({ -Math::x(forward), -Math::y(forward), -Math::z(forward), Math::inner(forward, farCenter) });

        // Correct Left plane calculation
        Math::Vec3f leftEdgeNear = (nearCenter - (right * (nearWidth / 2.0f))) - position;  // Vector from camera to the left edge of the near plane
        Math::Vec3f leftNormal = Math::normalized(Math::outer(up, leftEdgeNear));  // Cross product of the up vector and the left-edge vector
        frustumPlanes[2] = Math::Vec4f({ Math::x(leftNormal), Math::y(leftNormal), Math::z(leftNormal), -Math::inner(leftNormal, nearCenter) });

        // Correct Right plane calculation
        Math::Vec3f rightEdgeNear = (nearCenter + (right * (nearWidth / 2.0f))) - position;  // Vector from camera to the right edge of the near plane
        Math::Vec3f rightNormal = Math::normalized(Math::outer(rightEdgeNear, up));  // Cross product of the right-edge vector and the up vector
        frustumPlanes[3] = Math::Vec4f({ Math::x(rightNormal), Math::y(rightNormal), Math::z(rightNormal), -Math::inner(rightNormal, nearCenter) });

        // Correct Top plane calculation
        Math::Vec3f topEdgeNear = (nearCenter + (up * (nearHeight / 2.0f))) - position;  // Vector from camera to the top edge of the near plane
        Math::Vec3f topNormal = Math::normalized(Math::outer(right, topEdgeNear));  // Cross product of the right vector and the top-edge vector
        frustumPlanes[4] = Math::Vec4f({ Math::x(topNormal), Math::y(topNormal), Math::z(topNormal), -Math::inner(topNormal, nearCenter) });

        // Correct Bottom plane calculation
        Math::Vec3f bottomEdgeNear = (nearCenter - (up * (nearHeight / 2.0f))) - position;  // Vector from camera to the bottom edge of the near plane
        Math::Vec3f bottomNormal = Math::normalized(Math::outer(bottomEdgeNear, right));  // Cross product of the bottom-edge vector and the right vector
        frustumPlanes[5] = Math::Vec4f({ Math::x(bottomNormal), Math::y(bottomNormal), Math::z(bottomNormal), -Math::inner(bottomNormal, nearCenter) });


        // Step 4: Test the AABB against each frustum plane
        for (const auto& plane : frustumPlanes)
        {
            auto normal = Math::xyz(plane);
            float d = Math::w(plane);

            // Calculate positive and negative vertex for plane-AABB test
            Math::Vec3f positive = worldAABB.min;
            Math::Vec3f negative = worldAABB.max;

            if (Math::x(normal) >= 0) {
                Math::x(positive) = Math::x(worldAABB.max);
                Math::x(negative) = Math::x(worldAABB.min);
            }
            if (Math::y(normal) >= 0) {
                Math::y(positive) = Math::y(worldAABB.max);
                Math::y(negative) = Math::y(worldAABB.min);
            }
            if (Math::z(normal) >= 0) {
                Math::z(positive) = Math::z(worldAABB.max);
                Math::z(negative) = Math::z(worldAABB.min);
            }

            // Check if the positive vertex is outside the plane
            if (Math::inner(normal, positive) + d < 0) {
                // The AABB is outside the frustum
                return true;
            }

            if (Math::inner(normal, negative) + d >= 0)
            {
                return false;
            }
        }

        // If no planes exclude the AABB, it is inside the frustum
        return false;
    }


    /*
    bool Renderer::cull(
        const BoundingBox& aabb, 
        const mn::Math::Mat4<float>& model, 
        const Component::Transform& transform, 
        const Component::Camera& camera) const
    {
        using namespace mn::Math;

        // Construct the Frustum
        // Construct the Bounding box
        // Do the Separating Axis checks
        // return if success

        const auto center_pos_4 = model * Vec4f{ 0.f, 0.f, 0.f, 1.f };
        const auto center_pos = Vec3f{ x(center_pos_4), y(center_pos_4), z(center_pos_4)};
        
        return ( length(center_pos - transform.position) > 20.f );
    }*/
}
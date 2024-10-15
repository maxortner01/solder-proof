#include "Application.hpp"
#include "Component.hpp"
#include "../Util/DataRep.hpp"

#include <imgui.h>

namespace Engine
{
    void Scene::renderOverlay() const
    {
        ImGui::Begin("Entities");
        for (const auto& e : entities)
        {
            std::string text = std::string((e.name().size() ? e.name().c_str() : "Unnamed entity")) + " (id: " + std::to_string(e.raw_id()) + ")";
            if (ImGui::TreeNode(text.c_str()))
            {
                if (e.has<Component::Transform>())
                {
                    double min = -mn::Math::Angle::PI.asRadians();
                    double max =  mn::Math::Angle::PI.asRadians();
                    auto* transform = e.get_mut<Component::Transform>();
                    if (ImGui::TreeNode("Transform"))
                    {
                        ImGui::SliderFloat3("Position", (float*)&transform->position, -25.f, 25.f);
                        ImGui::SliderScalarN("Rotation", ImGuiDataType_Double, (double*)&transform->rotation, 3, &min, &max);
                        ImGui::SliderFloat3("Scale", (float*)&transform->scale, 0.f, 10.f);
                        ImGui::TreePop();
                    }
                }
                if (e.has<Component::Model>())
                {
                    auto* model = e.get_mut<Component::Model>();
                    if (ImGui::TreeNode("Model"))
                    {
                        ImGui::Text("Resource Name: %s", model->model.name.c_str());
                        ImGui::TreePop();
                    }
                }
                if (e.has<Component::Camera>())
                {
                    auto* camera = e.get_mut<Component::Camera>();
                    if (ImGui::TreeNode("Camera"))
                    {
                        double min = -mn::Math::Angle::PI.asRadians();
                        double max =  mn::Math::Angle::PI.asRadians();
                        ImGui::SliderScalar("FOV", ImGuiDataType_Double, (double*)&camera->FOV, &min, &max);
                        ImGui::SliderFloat2("Near/Far", (float*)&camera->near_far, 0.001f, 1000.f);
                        
                        const auto& color = camera->surface->getColorAttachments()[0];
                        const auto& depth = camera->surface->getDepthAttachment();

                        if (ImGui::BeginTable("texture_table", 2))
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            if (color.imgui_ds)
                            {
                                const auto ar = (float)mn::Math::x(color.size) / (float)mn::Math::y(color.size);
                                ImGui::Image((ImTextureID)color.imgui_ds, ImVec2(120 * ar, 120));
                            }

                            ImGui::TableSetColumnIndex(1);
                            if (depth.imgui_ds)
                            {
                                const auto ar = (float)mn::Math::x(depth.size) / (float)mn::Math::y(depth.size);
                                ImGui::Image((ImTextureID)depth.imgui_ds, ImVec2(120 * ar, 120));
                            }

                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("Size: (%u, %u)", mn::Math::x(color.size), mn::Math::y(color.size));

                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("Size: (%u, %u)", mn::Math::x(depth.size), mn::Math::y(depth.size));

                            ImGui::EndTable();
                        }
                        ImGui::TreePop();
                    }
                }
                if (e.has<Component::Light>())
                {
                    auto* light = e.get_mut<Component::Light>();
                    if (ImGui::TreeNode("Light"))
                    {
                        ImGui::ColorPicker3("Color", (float*)&light->color);
                        ImGui::SliderFloat("Intensity", (float*)&light->intensity, 0.f, 1000.f);
                        ImGui::TreePop();
                    }
                }

                ImGui::TreePop();
            }
        }
        ImGui::End();

        // Nicer way to do this
        ImGui::Begin("Resources");
        if (ImGui::CollapsingHeader("Models"))
        {
            auto model_map = res.get_type_map<Engine::Model>();
            for (const auto& [ name, model_ptr ] : model_map)
                if (ImGui::TreeNode(name.c_str()))
                {
                    const auto& meshes = model_ptr->getMeshes();
                    ImGui::Text("%lu kB", Util::convert<Util::Bytes, Util::Kilobytes>(model_ptr->allocated()));
                    ImGui::Text("Contains %lu meshes", meshes.size());
                    for (const auto& mesh : meshes) ImGui::Text("  %lu vertices, %lu indices", mesh->mesh->vertexCount(), mesh->mesh->indexCount());
                    ImGui::TreePop();
                }
        }
        if (ImGui::CollapsingHeader("Shaders"))
        {
            auto shader_map = res.get_type_map<mn::Graphics::Shader>();
            for (const auto& [ name, shader ] : shader_map)
                if (ImGui::TreeNode(name.c_str()))
                {
                    ImGui::Text("Shader type: %s", (shader->getType() == mn::Graphics::ShaderType::Vertex ? "Vertex" : "Fragment"));
                    ImGui::TreePop();
                }
        }
        if (ImGui::CollapsingHeader("Textures"))
        {
            auto texture_map = res.get_type_map<mn::Graphics::Texture>();
            for (const auto& [ name, texture ] : texture_map)
                if (ImGui::TreeNode(name.c_str()))
                {
                    if (ImGui::TreeNode("Attachments"))
                    {
                        const auto& color_attachments = texture->get_image()->getColorAttachments();
                        for (int i = 0; i < color_attachments.size(); i++)
                        {
                            if (ImGui::TreeNode((std::stringstream() << "Color Attachment " << i).str().c_str()))
                            {
                                ImGui::Text("Format: %u", color_attachments[i].format);
                                ImGui::Text("Handle: %p", color_attachments[i].handle);
                                if (color_attachments[i].imgui_ds)
                                    ImGui::Image(reinterpret_cast<ImTextureID>(color_attachments[i].imgui_ds), ImVec2(100, 100));
                                ImGui::TreePop();
                            }
                        }

                        if (texture->get_image()->hasDepthAttachment())
                        {
                            const auto& attachment = texture->get_image()->getDepthAttachment();
                            if (ImGui::TreeNode("Depth/Stencil Attachment"))
                            {
                                ImGui::Text("Format: %u", attachment.format);
                                ImGui::Text("Handle: %p", attachment.handle);
                                if (attachment.imgui_ds)
                                    ImGui::Image(reinterpret_cast<ImTextureID>(attachment.imgui_ds), ImVec2(100, 100));
                                ImGui::TreePop();
                            }
                        }
                        ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
        }
        ImGui::End();
    }
}
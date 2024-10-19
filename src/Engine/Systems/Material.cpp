#include "Material.hpp"

#include "Renderer.hpp"

namespace Engine::System
{
    DiffuseMaterial::DiffuseMaterial(ResourceManager& res) :
        layout(std::make_shared<mn::Graphics::Descriptor::Layout>([]()
        {
            using namespace mn::Graphics;
            DescriptorLayoutBuilder layout_builder;
            layout_builder.addBinding(Descriptor::Layout::Binding{ .type = Descriptor::Layout::Binding::Sampler, .count = 2 });
            layout_builder.addVariableBinding(Descriptor::Layout::Binding::Image, 4);
            return layout_builder.build();
        }()))
    {   
        using namespace mn::Graphics;

        auto descriptor_pool = Descriptor::Pool::make();
        default_desc = descriptor_pool->allocateDescriptor(layout);

        std::shared_ptr<Shader> vertex, fragment;
        if (!res.exists<Shader>("vertex.glsl"))
            vertex = res.create<Shader>("vertex.glsl", RES_DIR "/shaders/vertex.glsl", ShaderType::Vertex).value;
        else
            vertex = res.get<Shader>("vertex.glsl").value;

        if (!res.exists<Shader>("diffuse.fragment.glsl"))
            fragment = res.create<Shader>("diffuse.fragment.glsl", RES_DIR "/shaders/diffuse.fragment.glsl", ShaderType::Fragment).value;
        else
            fragment = res.get<Shader>("diffuse.fragment.glsl").value;

        // Build the pipeline
        pipeline = std::make_shared<Pipeline>(
            PipelineBuilder::fromLua(RES_DIR, "/shaders/main.lua")
                    .addShader(vertex)
                    .addShader(fragment)
                    .addDescriptorLayout(layout)
                    .setPushConstantObject<Renderer::PushConstant>()
                    .build());
    }

    Material::Instance
    DiffuseMaterial::resolveMaterial(const std::filesystem::path& base_path, aiMaterial* material) const 
    {
        using namespace mn::Graphics;

        const auto count = material->GetTextureCount(aiTextureType_DIFFUSE);
        if (!count) return { .set = default_desc, .pipeline = pipeline };
        
        for (uint32_t i = 0; i < count; i++)
        {
            aiString path;
            MIDNIGHT_ASSERT(material->GetTexture(aiTextureType_DIFFUSE, i, &path) == aiReturn_SUCCESS, "Error loading material");
            const auto complete_path = base_path.parent_path().string() + "/" + std::string(path.C_Str());
            
            // Load the texture
            textures.push_back(
                std::make_shared<Texture>(complete_path)
            );

            break;
        }

        // Create the descriptor set
        auto descriptor_pool = Descriptor::Pool::make();
        auto descriptor = descriptor_pool->allocateDescriptor(layout);

        // Update the descriptor set
        //   Assign the sampler to 0
        //   Assign textures.back() to 1
        auto& device = Backend::Instance::get()->getDevice();
        descriptor->update<Descriptor::Layout::Binding::Sampler>(0, { device->getSampler(Backend::Sampler::Linear)  });
        descriptor->update<Descriptor::Layout::Binding::Image>  (1, { textures.back()->get_image()                  });

        return Instance{ .set = descriptor, .pipeline = pipeline };
    }

    ColorMaterial::ColorMaterial(ResourceManager& res)
    {
        using namespace mn::Graphics;

        std::shared_ptr<Shader> vertex, fragment;
        if (!res.exists<Shader>("vertex.glsl"))
            vertex = res.create<Shader>("vertex.glsl", RES_DIR "/shaders/vertex.glsl", ShaderType::Vertex).value;
        else
            vertex = res.get<Shader>("vertex.glsl").value;

        if (!res.exists<Shader>("color.fragment.glsl"))
            fragment = res.create<Shader>("color.fragment.glsl", RES_DIR "/shaders/color.fragment.glsl", ShaderType::Fragment).value;
        else
            fragment = res.get<Shader>("color.fragment.glsl").value;

        // Build the pipeline
        pipeline = std::make_shared<mn::Graphics::Pipeline>(
            mn::Graphics::PipelineBuilder::fromLua(RES_DIR, "/shaders/main.lua")
                    .addShader(vertex)
                    .addShader(fragment)
                    .setPushConstantObject<Renderer::PushConstant>()
                    .setBackfaceCull(false)
                    .build());
    }

    Material::Instance
    ColorMaterial::resolveMaterial(const std::filesystem::path& base_path, aiMaterial* material) const
    {
        return Instance{ .set = nullptr, .pipeline = pipeline };
    }

    LineMaterial::LineMaterial(ResourceManager& res)
    {
        using namespace mn::Graphics;

        std::shared_ptr<Shader> vertex, fragment;
        if (!res.exists<Shader>("vertex.glsl"))
            vertex = res.create<Shader>("vertex.glsl", RES_DIR "/shaders/vertex.glsl", ShaderType::Vertex).value;
        else
            vertex = res.get<Shader>("vertex.glsl").value;

        if (!res.exists<Shader>("color.fragment.glsl"))
            fragment = res.create<Shader>("color.fragment.glsl", RES_DIR "/shaders/color.fragment.glsl", ShaderType::Fragment).value;
        else
            fragment = res.get<Shader>("color.fragment.glsl").value;

        // Build the pipeline
        pipeline = std::make_shared<mn::Graphics::Pipeline>(
            mn::Graphics::PipelineBuilder::fromLua(RES_DIR, "/shaders/main.lua")
                    .addShader(vertex)
                    .addShader(fragment)
                    .setPushConstantObject<Renderer::PushConstant>()
                    .setBackfaceCull(false)
                    .setTopology(Topology::Lines)
                    .build());
    }

    Material::Instance
    LineMaterial::resolveMaterial(const std::filesystem::path& base_path, aiMaterial* material) const 
    {
        return Instance{ .set = nullptr, .pipeline = pipeline };
    }
}
#include "Material.hpp"

namespace Engine::System
{
    DiffuseMaterial::DiffuseMaterial() :
        layout(std::make_shared<mn::Graphics::Descriptor::Layout>([]()
        {
            using namespace mn::Graphics;
            DescriptorLayoutBuilder layout_builder;
            layout_builder.addBinding(Descriptor::Layout::Binding{ .type = Descriptor::Layout::Binding::Sampler, .count = 2 });
            layout_builder.addVariableBinding(Descriptor::Layout::Binding::Image, 4);
            return layout_builder.build();
        }()))
    {   }

    std::shared_ptr<mn::Graphics::Descriptor> 
    DiffuseMaterial::resolveMaterial(const std::filesystem::path& base_path, aiMaterial* material) const 
    {
        using namespace mn::Graphics;

        const auto count = material->GetTextureCount(aiTextureType_DIFFUSE);
        if (!count) return nullptr;
        
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
        descriptor->update<Descriptor::Layout::Binding::Sampler>(0, { device->getSampler(Backend::Sampler::Nearest) });
        descriptor->update<Descriptor::Layout::Binding::Image>  (1, { textures.back()->get_image()                  });

        return descriptor;
    }
}
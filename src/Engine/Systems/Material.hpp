#pragma once

#include <midnight/midnight.hpp>
#include <assimp/scene.h>

#include <memory>

namespace Engine::System
{
    // Should be loaded in the resource manager
    struct Material
    {
        virtual std::shared_ptr<mn::Graphics::Descriptor> 
        resolveMaterial(const std::filesystem::path& base_path, aiMaterial* material) const = 0;
    };

    struct DiffuseMaterial : Material
    {
        std::shared_ptr<mn::Graphics::Descriptor::Layout> layout;

        DiffuseMaterial();

        std::shared_ptr<mn::Graphics::Descriptor> 
        resolveMaterial(const std::filesystem::path& base_path, aiMaterial* material) const override;

        mutable std::vector<std::shared_ptr<mn::Graphics::Texture>> textures;
    };
}
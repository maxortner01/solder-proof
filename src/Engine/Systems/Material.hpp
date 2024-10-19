#pragma once

#include <midnight/midnight.hpp>
#include <assimp/scene.h>

#include "../ResourceManager.hpp"

#include <memory>

namespace Engine::System
{
    // Should be loaded in the resource manager
    struct Material
    {
        struct Instance
        {
            std::shared_ptr<mn::Graphics::Pipeline> pipeline;
            std::shared_ptr<mn::Graphics::Descriptor> set;
        };

        virtual Instance 
        resolveMaterial(const std::filesystem::path& base_path, aiMaterial* material) const = 0;
    };

    struct DiffuseMaterial : Material
    {
        std::shared_ptr<mn::Graphics::Pipeline> pipeline;

        std::shared_ptr<mn::Graphics::Descriptor::Layout> layout;
        std::shared_ptr<mn::Graphics::Descriptor> default_desc;

        DiffuseMaterial(ResourceManager& res);

        Instance
        resolveMaterial(const std::filesystem::path& base_path, aiMaterial* material) const override;

        mutable std::vector<std::shared_ptr<mn::Graphics::Texture>> textures;
    };

    struct ColorMaterial : Material
    {
        std::shared_ptr<mn::Graphics::Pipeline> pipeline;

        ColorMaterial(ResourceManager& res);

        Instance
        resolveMaterial(const std::filesystem::path& base_path, aiMaterial* material) const override;
    };

    struct LineMaterial : Material
    {
        std::shared_ptr<mn::Graphics::Pipeline> pipeline;

        LineMaterial(ResourceManager& res);

        Instance
        resolveMaterial(const std::filesystem::path& base_path, aiMaterial* material) const override;
    };
}
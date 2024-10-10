#pragma once

#include <midnight/midnight.hpp>
#include <filesystem>

namespace Engine
{
    struct BoundingBox
    {
        mn::Math::Vec3f min, max;
    };

    struct Model
    {
        struct BoundedMesh
        {
            BoundingBox aabb;
            std::shared_ptr<mn::Graphics::Mesh> mesh;
        };

        Model() = default;
        Model(const Model&) = delete;
        Model(const std::filesystem::path& path);

        void loadFromFile(const std::filesystem::path& path);

        const auto& getMeshes() const { return _meshes; }

        std::size_t allocated() const;

    private:

        std::vector<BoundedMesh> _meshes;
    };
}
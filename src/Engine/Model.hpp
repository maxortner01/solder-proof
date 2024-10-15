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
            
            struct LOD
            {
                struct Level
                {
                    std::size_t offset, count;
                };

                std::vector<Level> lod_offsets;
                std::shared_ptr<mn::Graphics::TypeBuffer<uint32_t>> lod;
            } lods;
        };

        Model() = default;
        Model(const Model&) = delete;
        Model(const std::filesystem::path& path);

        void loadFromFile(const std::filesystem::path& path);

        const auto& getMeshes() const { return _meshes; }

        std::size_t allocated() const;

    private:
        void optimize_mesh();

        std::vector<std::shared_ptr<BoundedMesh>> _meshes;
    };
}
#pragma once

#include <midnight/midnight.hpp>
#include <filesystem>

#include "Systems/Material.hpp"

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
            std::shared_ptr<mn::Graphics::Descriptor> descriptor;
            
            struct LOD
            {
                struct Level
                {
                    std::size_t offset, count;
                };

                std::vector<Level> lod_offsets;
                std::shared_ptr<mn::Graphics::TypeBuffer<uint32_t>> lod;
                // TODO: We should not have a separate index buffer here
            } lods;
        };

        Model() = default;
        Model(const Model&) = delete;
        Model(const std::filesystem::path& path, std::shared_ptr<System::Material> material_sys = nullptr);

        void loadFromFile(const std::filesystem::path& path, std::shared_ptr<System::Material> material_sys = nullptr);

        const auto& getMeshes() const { return _meshes; }

        void drawUI() const;

        std::size_t allocated() const;

    private:
        void optimize_mesh();

        std::vector<std::shared_ptr<BoundedMesh>> _meshes;
    };
}
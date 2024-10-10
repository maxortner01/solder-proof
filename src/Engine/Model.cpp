#include "Model.hpp"

#include <type_traits>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace Engine
{
    Model::Model(const std::filesystem::path& path)
    {
        loadFromFile(path);
    }

    void Model::loadFromFile(const std::filesystem::path& path)
    {
        using namespace mn;
        using namespace mn::Graphics;

        Assimp::Importer importer;
        const auto* scene = importer.ReadFile(path.string(), 
            aiProcess_Triangulate | 
            aiProcess_FlipUVs | 
            aiProcess_GenNormals | 
            aiProcess_OptimizeMeshes |
            aiProcess_GenBoundingBoxes);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            std::cout << "Error loading model: " << path.filename().string() << "\n";
            return;
        }

        const auto transfer_vector = 
            [](auto& vec, const auto& in_vec)
            {
                Math::x(vec) = in_vec.x;
                Math::y(vec) = in_vec.y;
                if constexpr (std::decay_t<decltype(vec)>::Size == 3)
                    Math::z(vec) = in_vec.z;
            };

        const std::function<void(aiNode*, const aiScene*)>
            process = [this, &process, &transfer_vector](aiNode* node, const aiScene* scene) -> void
            {
                for (uint32_t i = 0; i < node->mNumMeshes; i++)
                {
                    auto* file_mesh = scene->mMeshes[node->mMeshes[i]];

                    // Could do this in a thread
                    // Process mesh
                    BoundingBox aabb;
                    transfer_vector(aabb.min, file_mesh->mAABB.mMin);
                    transfer_vector(aabb.max, file_mesh->mAABB.mMax);

                    Mesh::Frame frame;
                    frame.vertices.resize(file_mesh->mNumVertices);
                    for (uint32_t i = 0; i < file_mesh->mNumVertices; i++)
                    {
                        frame.vertices[i].color = { 1.f, 1.f, 1.f, 1.f };
                        transfer_vector(frame.vertices[i].position, file_mesh->mVertices[i]);
                        transfer_vector(frame.vertices[i].normal,   file_mesh->mNormals[i] );
                        if (file_mesh->mTextureCoords[0])
                            transfer_vector(frame.vertices[i].tex_coords, file_mesh->mTextureCoords[0][i]);
                    }

                    frame.indices.reserve(file_mesh->mNumFaces);
                    for (uint32_t i = 0; i < file_mesh->mNumFaces; i++)
                    {
                        const aiFace& face = file_mesh->mFaces[i];
                        for (uint32_t j = 0; j < face.mNumIndices; j++)
                            frame.indices.push_back(face.mIndices[j]);
                    }

                    this->_meshes.push_back(
                        BoundedMesh {
                            .aabb = aabb,
                            .mesh = std::make_shared<Mesh>(Mesh::fromFrame(frame))
                        }
                    );
                }   

                for (uint32_t i = 0; i < node->mNumChildren; i++)
                    process(node->mChildren[i], scene);
            };
        process(scene->mRootNode, scene);
    }

    std::size_t Model::allocated() const
    {
        return std::reduce(_meshes.begin(), _meshes.end(), 0U, [](auto acc, const auto& mesh) { return acc + mesh.mesh->allocated(); });
    }
}
#include "Model.hpp"

#include "../Util/DataRep.hpp"

#include <type_traits>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <imgui.h>

#include <meshoptimizer.h>

namespace Engine
{
    Model::Model(const std::filesystem::path& path, std::shared_ptr<System::Material> material_sys)
    {
        loadFromFile(path, material_sys);
    }

    
    std::shared_ptr<Model::BoundedMesh> Model::pushMesh(const std::shared_ptr<mn::Graphics::Mesh>& mesh)
    {
        auto model = std::make_shared<BoundedMesh>();
        model->mesh = mesh;
        
        const auto vertex_span = mesh->vertices();
        for (const auto& vertex : vertex_span)
        {
            model->aabb.min = mn::Math::min(model->aabb.min, vertex.position);
            model->aabb.max = mn::Math::max(model->aabb.max, vertex.position);
        }

        _meshes.push_back(model);

        return model;
    }

    void Model::loadFromFile(const std::filesystem::path& path, std::shared_ptr<System::Material> material_sys)
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
            process = [&, this](aiNode* node, const aiScene* scene) -> void
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

                    System::Material::Instance material;
                    if (scene->HasMaterials() && file_mesh->mMaterialIndex < scene->mNumMaterials && material_sys.get())
                        material = material_sys->resolveMaterial(path, scene->mMaterials[file_mesh->mMaterialIndex]);

                    this->_meshes.push_back(std::make_shared<BoundedMesh>(
                        BoundedMesh {
                            .aabb = aabb,
                            .mesh = std::make_shared<Mesh>(Mesh::fromFrame(frame)),
                            .material = material
                        })
                    );
                }   

                for (uint32_t i = 0; i < node->mNumChildren; i++)
                    process(node->mChildren[i], scene);
            };
        process(scene->mRootNode, scene);

        optimize_mesh();
    }

    std::size_t Model::allocated() const
    {
        return std::reduce(_meshes.begin(), _meshes.end(), 0U, [](auto acc, const auto& mesh) { return acc + mesh->mesh->allocated(); });
    }

    void Model::optimize_mesh()
    {
        for (auto& mesh : _meshes)
        {
            auto vertices = mesh->mesh->vertices(); 
            auto indices  = mesh->mesh->indices();
            meshopt_optimizeVertexCache(
                &indices[0],
                &indices[0], 
                indices.size(),
                mesh->mesh->vertexCount());
            
            meshopt_optimizeOverdraw(
                &indices[0],
                &indices[0],
                indices.size(),
                (float*)&vertices[0],
                vertices.size(),
                sizeof(mn::Graphics::Mesh::Vertex),
                1.05f
            );

            meshopt_optimizeVertexFetch(
                &vertices[0],
                &indices[0],
                indices.size(),
                &vertices[0],
                vertices.size(),
                sizeof(mn::Graphics::Mesh::Vertex)
            );

            const float thresholds[] = {
                0.001f, 0.01f, 0.1f, 0.2f, 0.5f
            };

            std::size_t index = 0;
            std::vector<uint32_t> sandbox(indices.size());
            std::vector<uint32_t> final_indices;
            for (int i = 0; i < 5; i++)
            {
                size_t target_index_count = size_t(indices.size() * thresholds[i]);
                float target_error = 1e-1f;

                float lod_error = 0.f;
                const auto size = meshopt_simplifySloppy(
                    &sandbox[0],
                    &indices[0],
                    indices.size(),
                    (float*)&vertices[0],
                    vertices.size(),
                    sizeof(mn::Graphics::Mesh::Vertex),
                    target_index_count,
                    target_error,
                    &lod_error
                );
                meshopt_optimizeVertexCache(&sandbox[0], &sandbox[0], size, vertices.size());
                std::copy(sandbox.begin(), sandbox.begin() + size, std::back_inserter(final_indices));
                mesh->lods.lod_offsets.push_back(BoundedMesh::LOD::Level { .offset = index, .count = size });
                index += size;
            }
            
            mesh->lods.lod = std::make_shared<mn::Graphics::TypeBuffer<uint32_t>>();
            mesh->lods.lod->resize(final_indices.size());
            std::copy(final_indices.begin(), final_indices.end(), &mesh->lods.lod->at(0));
        }
    }

    void Model::drawUI() const
    {
        std::size_t total_byte_size = 0;
        for (const auto& mesh : _meshes)
            total_byte_size += mesh->mesh->allocated() + mesh->lods.lod->allocated();

        ImGui::Text("Total GPU Allocation: %s kB", Util::withCommas( Util::convert<Util::Bytes, Util::Kilobytes>(total_byte_size) ).c_str());
        for (int i = 0; i < _meshes.size(); i++)
        {
            if (ImGui::TreeNode((std::stringstream() << "Mesh " << i + 1).str().c_str()))
            {
                ImGui::Text("Vertex Count: %s", Util::withCommas(_meshes[i]->mesh->vertexCount()).c_str());
                ImGui::Text("Base Index Count: %s", Util::withCommas(_meshes[i]->mesh->indexCount()).c_str());

                ImGui::SeparatorText((std::stringstream() << "LOD levels: " << _meshes[i]->lods.lod_offsets.size()).str().c_str());
                if (_meshes[i]->lods.lod_offsets.size())
                {
                    ImGui::BeginTable("LodOffsets", 2);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Level");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("Index Count");

                    for (int j = 0; j < _meshes[i]->lods.lod_offsets.size(); j++)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%i", j);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%lu", _meshes[i]->lods.lod_offsets[j].count);
                    }
                    ImGui::EndTable();
                }

                ImGui::TreePop();
            }
        }
    }
}
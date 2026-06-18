#include "render/Model.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <SDL2/SDL.h>

#include <cstdio>
#include <functional>

namespace sims {

namespace {

glm::mat4 ai_to_glm(const aiMatrix4x4& m) {
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4);
}

} // namespace

bool Model::load_from_file(const std::string& path) {
    meshes_.clear();
    textures_.clear();
    directory_ = path.substr(0, path.find_last_of('/'));

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::fprintf(stderr, "[model] Assimp error: %s\n", importer.GetErrorString());
        return false;
    }

    std::function<void(aiNode*)> walk = [&](aiNode* node) {
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            aiMesh* am = scene->mMeshes[node->mMeshes[i]];
            std::vector<Vertex> verts;
            std::vector<unsigned int> idx;
            verts.reserve(am->mNumVertices);
            for (unsigned int v = 0; v < am->mNumVertices; ++v) {
                Vertex vert;
                vert.position = { am->mVertices[v].x, am->mVertices[v].y, am->mVertices[v].z };
                vert.normal = am->mNormals
                    ? glm::vec3(am->mNormals[v].x, am->mNormals[v].y, am->mNormals[v].z)
                    : glm::vec3(0.0f, 1.0f, 0.0f);
                vert.texcoord = am->mTextureCoords[0]
                    ? glm::vec2(am->mTextureCoords[0][v].x, am->mTextureCoords[0][v].y)
                    : glm::vec2(0.0f);
                verts.push_back(vert);
            }
            for (unsigned int f = 0; f < am->mNumFaces; ++f) {
                const aiFace& face = am->mFaces[f];
                for (unsigned int j = 0; j < face.mNumIndices; ++j) idx.push_back(face.mIndices[j]);
            }
            ModelMesh mm;
            mm.mesh.upload(verts, idx);
            mm.transform = ai_to_glm(node->mTransformation);

            if (am->mMaterialIndex < scene->mNumMaterials) {
                aiMaterial* mat = scene->mMaterials[am->mMaterialIndex];
                aiColor4D diff(1.0f, 1.0f, 1.0f, 1.0f);
                if (aiGetMaterialColor(mat, AI_MATKEY_BASE_COLOR, &diff) != AI_SUCCESS) {
                    aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &diff);
                }
                mm.base_color = {diff.r, diff.g, diff.b, diff.a};
                aiTextureType ttype = mat->GetTextureCount(aiTextureType_BASE_COLOR) > 0
                    ? aiTextureType_BASE_COLOR : aiTextureType_DIFFUSE;
                if (mat->GetTextureCount(ttype) > 0) {
                    aiString texpath;
                    mat->GetTexture(ttype, 0, &texpath);
                    std::string full = directory_ + "/" + texpath.C_Str();
                    auto tex = std::make_unique<Texture>();
                    if (tex->load_from_file(full, true)) {
                        mm.texture = tex.get();
                        textures_.push_back(std::move(tex));
                    }
                }
            }
            meshes_.push_back(std::move(mm));
        }
        for (unsigned int c = 0; c < node->mNumChildren; ++c) walk(node->mChildren[c]);
    };
    walk(scene->mRootNode);
    importer.FreeScene();
    return !meshes_.empty();
}

void Model::draw() const {
    for (const ModelMesh& mm : meshes_) {
        if (mm.texture) mm.texture->bind(0);
        mm.mesh.draw();
    }
}

} // namespace sims

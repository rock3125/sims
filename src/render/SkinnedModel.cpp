#include "render/SkinnedModel.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <SDL2/SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>

namespace sims {

namespace {

glm::mat4 ai_to_glm(const aiMatrix4x4& m) {
    // Assimp matrices are row-major; glm is column-major. This transpose-free
    // construction mirrors the existing Model.cpp conversion.
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4);
}

glm::quat ai_quat_to_glm(const aiQuaternion& q) {
    return glm::quat(q.w, q.x, q.y, q.z);
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string basename_of(const std::string& p) {
    // Strip directories and any trailing "\\..\\mixamo\\..." style hints that
    // Assimp sometimes leaves on FBX texture paths. We only want the file leaf.
    auto fwd = p.find_last_of('/');
    auto bwd = p.find_last_of('\\');
    size_t pos = std::string::npos;
    if (fwd != std::string::npos && bwd != std::string::npos) pos = std::max(fwd, bwd);
    else if (fwd != std::string::npos) pos = fwd;
    else if (bwd != std::string::npos) pos = bwd;
    std::string leaf = (pos == std::string::npos) ? p : p.substr(pos + 1);
    // Trim trailing "." and any "*N" embedded-index markers.
    if (!leaf.empty() && leaf.front() == '*') return leaf;
    // Strip a "(\0embedded\0...)" hint prefix if present.
    if (leaf.size() >= 2 && leaf[0] == '(' && leaf[1] == '\0') return std::string{};
    return leaf;
}

// Decode an Assimp aiTexture (either compressed or raw RGBA) into an owning
// Texture and register it in `out_owner`; return the raw pointer.
Texture* decode_embedded(const aiTexture* et,
                         std::vector<std::unique_ptr<Texture>>& out_owner) {
    auto tex = std::make_unique<Texture>();
    bool ok = false;
    if (et->mHeight == 0) {
        // Compressed (PNG/JPG/...). mWidth is the byte length.
        ok = tex->load_from_memory(
            reinterpret_cast<const unsigned char*>(et->pcData),
            static_cast<int>(et->mWidth), true);
    } else {
        // Raw RGBA. mWidth*mHeight pixels of aiTexel (4 bytes each).
        ok = tex->load_from_memory(
            reinterpret_cast<const unsigned char*>(et->pcData),
            static_cast<int>(et->mWidth * et->mHeight * 4), true);
    }
    if (!ok) return nullptr;
    Texture* raw_p = tex.get();
    out_owner.push_back(std::move(tex));
    return raw_p;
}

// Resolve a material's texture to an owning Texture. Order of preference:
//   1) Embedded texture in scene->mTextures[] referenced by `*N` index path
//       or by basename match against mTextures[i].mFilename.
//   2) File on disk relative to the model directory (or absolute if path is
//       already absolute) — non-embedded glTFs / loose-texture FBX exports.
Texture* resolve_texture(const aiScene* scene, aiMaterial* mat, const std::string& dir,
                         std::vector<std::unique_ptr<Texture>>& out_owner) {
    aiTextureType ttype = mat->GetTextureCount(aiTextureType_BASE_COLOR) > 0
        ? aiTextureType_BASE_COLOR : aiTextureType_DIFFUSE;
    if (mat->GetTextureCount(ttype) == 0) return nullptr;

    aiString texpath;
    if (mat->GetTexture(ttype, 0, &texpath) != AI_SUCCESS) return nullptr;
    std::string raw = texpath.C_Str();

    // 1a) Direct embedded-index reference: "*N" → scene->mTextures[N].
    if (!raw.empty() && raw.front() == '*') {
        try {
            int idx = std::stoi(raw.substr(1));
            if (idx >= 0 && static_cast<unsigned>(idx) < scene->mNumTextures) {
                if (auto* t = decode_embedded(scene->mTextures[idx], out_owner)) return t;
            }
        } catch (...) { /* fall through */ }
    }

    // 1b) Match by basename against scene->mTextures[]. FBX often leaves the
    //    original server-side path in the material but stores the embedded
    //    blob under its leaf name (e.g. "Ch22_1001_Diffuse.png").
    std::string want = lower(basename_of(raw));
    if (!want.empty()) {
        for (unsigned int i = 0; i < scene->mNumTextures; ++i) {
            const aiTexture* et = scene->mTextures[i];
            if (lower(basename_of(et->mFilename.C_Str())) == want) {
                if (auto* t = decode_embedded(et, out_owner)) return t;
            }
        }
    }

    // 2) Disk fallback.
    namespace fs = std::filesystem;
    fs::path p = raw;
    fs::path full = p.is_absolute() ? p : (fs::path(dir) / p);
    auto tex = std::make_unique<Texture>();
    if (tex->load_from_file(full.string(), true)) {
        Texture* raw_p = tex.get();
        out_owner.push_back(std::move(tex));
        return raw_p;
    }
    return nullptr;
}

} // namespace

bool SkinnedModel::load_from_file(const std::string& path) {
    meshes_.clear();
    textures_.clear();
    skeleton_ = Skeleton{};
    clip_names_.clear();
    directory_ = path.substr(0, path.find_last_of('/'));

    Assimp::Importer importer;
    // aiProcess_EmbedTextures keeps embedded FBX/glTF media in scene->mTextures[]
    //   (Mixamo FBX stores textures as embedded Video objects with absolute
    //   server-side filename strings — only the embedded blobs are usable).
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices | aiProcess_PopulateArmatureData |
        aiProcess_LimitBoneWeights | aiProcess_EmbedTextures);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::fprintf(stderr, "[skinned] Assimp error: %s\n", importer.GetErrorString());
        return false;
    }
    std::printf("[skinned] scene: %u meshes, %u embedded textures, %u animations\n",
                scene->mNumMeshes, scene->mNumTextures, scene->mNumAnimations);

    // 1) Build the node tree (armature + scene nodes) from the root.
    std::function<void(aiNode*, int)> walk_nodes = [&](aiNode* node, int parent) {
        int idx = skeleton_.add_node(node->mName.C_Str(), parent,
                                     ai_to_glm(node->mTransformation));
        for (unsigned int c = 0; c < node->mNumChildren; ++c) {
            walk_nodes(node->mChildren[c], idx);
        }
    };
    walk_nodes(scene->mRootNode, -1);

    // 2) Process every mesh: vertices, bone weights, material.
    for (unsigned int mi = 0; mi < scene->mNumMeshes; ++mi) {
        aiMesh* am = scene->mMeshes[mi];

        // Register bones for this mesh (idempotent across meshes).
        bool has_bones = am->mNumBones > 0;
        std::vector<int> mesh_bone_ids; // maps this mesh's aiBone index → global bone idx
        mesh_bone_ids.resize(am->mNumBones, -1);
        for (unsigned int b = 0; b < am->mNumBones; ++b) {
            aiBone* aib = am->mBones[b];
            std::string bname = aib->mName.C_Str();
            int existing = skeleton_.bone_index(bname);
            if (existing < 0) {
                int node_idx = skeleton_.node_index(bname);
                existing = skeleton_.add_bone(bname, node_idx,
                                              ai_to_glm(aib->mOffsetMatrix));
            }
            mesh_bone_ids[b] = existing;
        }

        std::vector<SkinnedVertex> verts;
        std::vector<unsigned int> idx;
        verts.reserve(am->mNumVertices);
        for (unsigned int v = 0; v < am->mNumVertices; ++v) {
            SkinnedVertex sv;
            sv.position = {am->mVertices[v].x, am->mVertices[v].y, am->mVertices[v].z};
            sv.normal = am->mNormals
                ? glm::vec3(am->mNormals[v].x, am->mNormals[v].y, am->mNormals[v].z)
                : glm::vec3(0.0f, 1.0f, 0.0f);
            sv.texcoord = am->mTextureCoords[0]
                ? glm::vec2(am->mTextureCoords[0][v].x, am->mTextureCoords[0][v].y)
                : glm::vec2(0.0f);
            verts.push_back(sv);
        }
        for (unsigned int f = 0; f < am->mNumFaces; ++f) {
            const aiFace& face = am->mFaces[f];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) idx.push_back(face.mIndices[j]);
        }

        // Apply bone weights. With aiProcess_LimitBoneWeights each vertex has
        // at most 4 influences; accumulate by slot and normalize afterwards.
        if (has_bones) {
            std::vector<int> counts(am->mNumVertices, 0);
            for (unsigned int b = 0; b < am->mNumBones; ++b) {
                aiBone* aib = am->mBones[b];
                int bone_idx = mesh_bone_ids[b];
                for (unsigned int w = 0; w < aib->mNumWeights; ++w) {
                    const aiVertexWeight& vw = aib->mWeights[w];
                    if (vw.mVertexId >= am->mNumVertices) continue;
                    int slot = counts[vw.mVertexId]++;
                    if (slot >= 4) continue; // safety; LimitBoneWeights should prevent this
                    verts[vw.mVertexId].bone_ids[slot] = bone_idx;
                    verts[vw.mVertexId].bone_weights[slot] = vw.mWeight;
                }
            }
            for (auto& v : verts) {
                float sum = v.bone_weights.x + v.bone_weights.y +
                            v.bone_weights.z + v.bone_weights.w;
                if (sum > 1e-6f) v.bone_weights /= sum;
                else { v.bone_ids = {0, 0, 0, 0}; v.bone_weights = {1, 0, 0, 0}; }
            }
        }

        SkinnedModelMesh mm;
        mm.has_bones = has_bones;
        mm.mesh.upload_skinned(verts, idx);

        // Material (base color + texture). Embedded FBX textures are pulled
        // from scene->mTextures[] by resolve_texture; loose glTFs fall back
        // to disk paths relative to the model file.
        if (am->mMaterialIndex < scene->mNumMaterials) {
            aiMaterial* mat = scene->mMaterials[am->mMaterialIndex];
            aiColor4D diff(1.0f, 1.0f, 1.0f, 1.0f);
            if (aiGetMaterialColor(mat, AI_MATKEY_BASE_COLOR, &diff) != AI_SUCCESS) {
                aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &diff);
            }
            mm.base_color = {diff.r, diff.g, diff.b, diff.a};
            mm.texture = resolve_texture(scene, mat, directory_, textures_);
        }
        meshes_.push_back(std::move(mm));
    }

    // 3) Import animation clips embedded in the avatar file.
    import_clips(scene, /*name_override=*/"");

    importer.FreeScene();
    std::printf("[skinned] loaded %s: %zu meshes, %zu bones, %zu nodes, %zu clips\n",
                path.c_str(), meshes_.size(), skeleton_.bones().size(),
                skeleton_.nodes().size(), skeleton_.clips().size());
    return !meshes_.empty();
}

bool SkinnedModel::load_animations_from_file(const std::string& path,
                                             const std::string& clip_name_override) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices);
    if (!scene) {
        std::fprintf(stderr, "[skinned] anim load failed for %s: %s\n",
                     path.c_str(), importer.GetErrorString());
        return false;
    }
    std::size_t before = skeleton_.clips().size();
    import_clips(scene, clip_name_override);
    importer.FreeScene();
    std::size_t added = skeleton_.clips().size() - before;
    std::printf("[skinned] merged %zu clip(s) from %s (total: %zu)\n",
                added, path.c_str(), skeleton_.clips().size());
    return added > 0;
}

void SkinnedModel::import_clips(const aiScene* scene, const std::string& name_override) {
    for (unsigned int a = 0; a < scene->mNumAnimations; ++a) {
        aiAnimation* anim = scene->mAnimations[a];
        AnimationClip clip;
        clip.name = !name_override.empty()
            ? name_override
            : std::string(anim->mName.C_Str());
        if (clip.name.empty()) clip.name = "anim_" + std::to_string(skeleton_.clips().size());
        clip.duration_ticks = anim->mDuration;
        clip.ticks_per_second = anim->mTicksPerSecond != 0.0 ? anim->mTicksPerSecond : 25.0;
        for (unsigned int ch = 0; ch < anim->mNumChannels; ++ch) {
            aiNodeAnim* na = anim->mChannels[ch];
            AnimationChannel channel;
            channel.node_name = na->mNodeName.C_Str();
            for (unsigned int k = 0; k < na->mNumPositionKeys; ++k) {
                auto& pk = na->mPositionKeys[k];
                channel.position_keys.push_back({static_cast<float>(pk.mTime),
                    glm::vec3(pk.mValue.x, pk.mValue.y, pk.mValue.z)});
            }
            for (unsigned int k = 0; k < na->mNumRotationKeys; ++k) {
                auto& rk = na->mRotationKeys[k];
                channel.rotation_keys.push_back({static_cast<float>(rk.mTime),
                    ai_quat_to_glm(rk.mValue)});
            }
            for (unsigned int k = 0; k < na->mNumScalingKeys; ++k) {
                auto& sk = na->mScalingKeys[k];
                channel.scale_keys.push_back({static_cast<float>(sk.mTime),
                    glm::vec3(sk.mValue.x, sk.mValue.y, sk.mValue.z)});
            }
            clip.channels.push_back(std::move(channel));
        }
        clip_names_.push_back(clip.name);
        skeleton_.add_clip(std::move(clip));
    }
}

void SkinnedModel::draw(const std::vector<glm::mat4>& bone_matrices) const {
    (void)bone_matrices;
    for (const SkinnedModelMesh& mm : meshes_) {
        if (mm.texture) mm.texture->bind(0);
        mm.mesh.draw();
    }
}

} // namespace sims

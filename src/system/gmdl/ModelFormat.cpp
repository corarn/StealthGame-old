#include "ModelFormat.h"

namespace gm31::model {

Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b) noexcept
{
    Matrix4x4 r{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            r.m[i][j] = a.m[i][0] * b.m[0][j]
                      + a.m[i][1] * b.m[1][j]
                      + a.m[i][2] * b.m[2][j]
                      + a.m[i][3] * b.m[3][j];
        }
    }
    return r;
}

std::vector<Matrix4x4> ComputeBindPoseWorldTransforms(const Model& model)
{
    std::vector<Matrix4x4> world(model.nodes.size());
    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        const Node& node = model.nodes[i];
        // pre-order 格納なので parent は必ず自分より前 → 既に確定している
        world[i] = (node.parent == kInvalidIndex)
                 ? node.localTransform
                 : Multiply(node.localTransform, world[static_cast<std::size_t>(node.parent)]);
    }
    return world;
}

std::vector<Matrix4x4>
ComputeSkinningMatrices(const Model& model, const std::vector<Matrix4x4>& nodeWorldTransforms)
{
    std::vector<Matrix4x4> palette(model.bones.size());
    for (std::size_t i = 0; i < model.bones.size(); ++i) {
        const std::int32_t nodeIndex = model.bones[i].nodeIndex;
        if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= nodeWorldTransforms.size()) {
            continue;   // 単位行列のまま（Validate で弾かれる想定）
        }
        palette[i] = Multiply(model.bones[i].offsetMatrix,
                              nodeWorldTransforms[static_cast<std::size_t>(nodeIndex)]);
    }
    return palette;
}

std::int32_t FindNodeIndex(const Model& model, std::string_view name) noexcept
{
    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        if (model.nodes[i].name == name) {
            return static_cast<std::int32_t>(i);
        }
    }
    return kInvalidIndex;
}

bool Validate(const Model& model, std::vector<std::string>& problems)
{
    const std::size_t problemsBefore = problems.size();
    const auto add = [&problems](std::string message) { problems.push_back(std::move(message)); };

    // ノードは pre-order 前提（parent < self）。ここが崩れると
    // ComputeBindPoseWorldTransforms が未確定の親を参照してしまう。
    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        const std::int32_t parent = model.nodes[i].parent;
        if (parent == kInvalidIndex) continue;
        if (parent < 0 || static_cast<std::size_t>(parent) >= model.nodes.size()) {
            add("node[" + std::to_string(i) + "] の親インデックスが範囲外です");
        } else if (static_cast<std::size_t>(parent) >= i) {
            add("node[" + std::to_string(i) + "] の親が自分より後ろにあります（pre-order 違反）");
        }
    }

    for (std::size_t i = 0; i < model.bones.size(); ++i) {
        const std::int32_t nodeIndex = model.bones[i].nodeIndex;
        if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= model.nodes.size()) {
            add("bone[" + std::to_string(i) + "] のノードインデックスが範囲外です");
        }
    }

    for (std::size_t i = 0; i < model.subsets.size(); ++i) {
        const Subset& s = model.subsets[i];
        const std::string tag = "subset[" + std::to_string(i) + "] ";

        if (s.indexCount % 3 != 0) {
            add(tag + "のインデックス数が 3 の倍数ではありません");
        }
        if (static_cast<std::size_t>(s.indexOffset) + s.indexCount > model.indices.size()) {
            add(tag + "のインデックス範囲がバッファ外です");
        }
        if (static_cast<std::size_t>(s.vertexOffset) + s.vertexCount > model.vertices.size()) {
            add(tag + "の頂点範囲がバッファ外です");
        }
        if (s.materialIndex >= model.materials.size()) {
            add(tag + "のマテリアルインデックスが範囲外です");
        }
    }

    if (!model.pbrMaterials.empty() && model.pbrMaterials.size() != model.materials.size()) {
        add("pbrMaterials の数が materials と一致しません ("
            + std::to_string(model.pbrMaterials.size()) + " / "
            + std::to_string(model.materials.size()) + ")");
    }

    {
        // 全インデックスの範囲検査。Debug ビルドの vector::operator[] は毎回
        // 境界チェックが入って遅いので、生ポインタで回す（検査内容は同じ）。
        const std::uint32_t* indices     = model.indices.data();
        const std::size_t    indexCount  = model.indices.size();
        const std::size_t    vertexCount = model.vertices.size();
        for (std::size_t i = 0; i < indexCount; ++i) {
            if (indices[i] >= vertexCount) {
                add("index[" + std::to_string(i) + "] が頂点数を超えています");
                break;   // 1 件出れば十分（全件出すとログが溢れる）
            }
        }
    }

    if (!model.bones.empty()) {
        // こちらも全頂点 × ボーン 4 本を見るので生ポインタで回す。
        const Vertex*     vertices    = model.vertices.data();
        const std::size_t vertexCount = model.vertices.size();
        const std::size_t boneCount   = model.bones.size();
        for (std::size_t i = 0; i < vertexCount; ++i) {
            const Vertex& v = vertices[i];
            float total = 0.0f;
            for (int b = 0; b < kMaxBoneInfluence; ++b) {
                if (v.boneWeight[b] > 0.0f && v.boneIndex[b] >= boneCount) {
                    add("vertex[" + std::to_string(i) + "] のボーンインデックスが範囲外です");
                    return problems.size() == problemsBefore;
                }
                total += v.boneWeight[b];
            }
            // ボーン非影響頂点（total == 0）は許容する
            if (total > 0.0f && (total < 0.99f || total > 1.01f)) {
                add("vertex[" + std::to_string(i) + "] のウェイト合計が 1.0 ではありません ("
                    + std::to_string(total) + ")");
                break;
            }
        }
    }

    for (std::size_t a = 0; a < model.animations.size(); ++a) {
        for (std::size_t c = 0; c < model.animations[a].channels.size(); ++c) {
            const std::int32_t nodeIndex = model.animations[a].channels[c].nodeIndex;
            if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= model.nodes.size()) {
                add("animation[" + std::to_string(a) + "].channel[" + std::to_string(c) +
                    "] のノードインデックスが範囲外です");
            }
        }
    }

    return problems.size() == problemsBefore;
}

} // namespace gm31::model

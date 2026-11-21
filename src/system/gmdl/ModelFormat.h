//==============================================================================
// ModelFormat.h
//
//  自前モデルフォーマット（.gmdl）のランタイム表現。
//
//  設計方針
//    - assimp にも DirectX にも依存しない。<cstdint> / <string> / <vector> のみ。
//      → コンバータ（assimp あり）とランタイム（assimp なし）の両方から使える。
//    - 行列は「行ベクトル規約・行優先格納」（DirectX / DirectXMath と同じ）。
//      translation は m[3][0..2] に入る。assimp は列ベクトル規約なので、
//      インポート時に転置している（AssimpImporter.cpp の ToMatrix4x4 を参照）。
//    - 頂点・インデックスはモデル全体で 1 本のバッファに連結し、
//      Subset が範囲（vertexOffset / indexOffset）で切り出す。
//      indices はサブセットローカルではなく **グローバル頂点番号**。
//      つまり DrawIndexed(indexCount, indexOffset, 0) で描ける。
//    - ノード階層はポインタツリーではなく「親インデックス付きの平坦配列」。
//      pre-order で格納するため常に parent < self が成り立ち、
//      ワールド行列は前から 1 パス回すだけで計算できる（再帰不要・キャッシュ効率良）。
//      シリアライズもそのまま memcpy でよい。
//==============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gm31::model {

//------------------------------------------------------------------------------
// フォーマット定数
//------------------------------------------------------------------------------

/// ファイル先頭のマジック "GM31MDL\0"
inline constexpr char kMagic[8] = { 'G', 'M', '3', '1', 'M', 'D', 'L', '\0' };

/// フォーマットバージョン。互換性を壊す変更のたびに上げる。
inline constexpr std::uint32_t kVersion = 1;

/// 1 頂点あたりのボーン影響数（スキニング行列パレットの参照数）
inline constexpr int kMaxBoneInfluence = 4;

/// ノードに対応するボーンが無いことを示す値
inline constexpr std::int32_t kInvalidIndex = -1;

//------------------------------------------------------------------------------
// 基本型（POD。そのままバイナリに落ちる）
//------------------------------------------------------------------------------

struct Float2 { float x = 0.0f, y = 0.0f; };
struct Float3 { float x = 0.0f, y = 0.0f, z = 0.0f; };
struct Float4 { float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f; };

/// クォータニオン（x, y, z, w の順。w がスカラー部）
struct Quaternion { float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f; };

/// 4x4 行列。行ベクトル規約・行優先格納（m[row][col]、平行移動は m[3][*]）。
struct Matrix4x4 {
    float m[4][4] = {
        { 1.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f },
    };
};

//------------------------------------------------------------------------------
// 頂点
//------------------------------------------------------------------------------

/// 頂点 1 つ。104 バイト固定長・パディング無しの POD。
/// ボーンを持たないモデルでは boneWeight が全て 0 になる。
struct Vertex {
    Float3 position{};
    Float3 normal{};
    Float3 tangent{};
    Float4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    Float2 uv{};

    /// Model::bones への添字。未使用スロットは 0（weight が 0 なので影響しない）。
    std::uint32_t boneIndex[kMaxBoneInfluence] = { 0, 0, 0, 0 };
    /// 合計が 1.0 になるよう正規化済み（ボーン無しの場合のみ全て 0）。
    float boneWeight[kMaxBoneInfluence] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

//------------------------------------------------------------------------------
// サブセット（= 元の 1 メッシュ。1 マテリアルで描画できる単位）
//------------------------------------------------------------------------------

struct Subset {
    std::string   name;                      ///< 元のメッシュ名
    std::uint32_t materialIndex = 0;         ///< Model::materials への添字
    std::uint32_t vertexOffset  = 0;         ///< Model::vertices 内の先頭位置
    std::uint32_t vertexCount   = 0;
    std::uint32_t indexOffset   = 0;         ///< Model::indices 内の先頭位置
    std::uint32_t indexCount    = 0;         ///< 必ず 3 の倍数

    /// このサブセットを参照していたノード（Model::nodes への添字）。
    std::int32_t  nodeIndex     = kInvalidIndex;

    /// true ならスキニング（ボーン）で動く。
    ///  - hasSkin == false : 頂点は既にモデル空間へ焼き込み済み。そのまま描画してよい。
    ///  - hasSkin == true  : 頂点はバインド空間のまま。スキニング行列で変形させること。
    bool          hasSkin       = false;
};

//------------------------------------------------------------------------------
// マテリアル
//------------------------------------------------------------------------------

/// テクスチャ参照。埋め込みテクスチャの場合は embeddedIndex が有効、
/// 外部ファイルの場合は path（モデルファイルからの相対パス）が有効。
struct TextureRef {
    std::string  path;                            ///< 外部ファイルの相対パス（無ければ空）
    std::int32_t embeddedIndex = kInvalidIndex;   ///< Model::embeddedTextures への添字

    [[nodiscard]] bool IsValid() const noexcept {
        return !path.empty() || embeddedIndex != kInvalidIndex;
    }
    [[nodiscard]] bool IsEmbedded() const noexcept {
        return embeddedIndex != kInvalidIndex;
    }
};

struct Material {
    std::string name;

    Float4 ambient  { 0.0f, 0.0f, 0.0f, 1.0f };
    Float4 diffuse  { 1.0f, 1.0f, 1.0f, 1.0f };
    Float4 specular { 0.0f, 0.0f, 0.0f, 1.0f };
    Float4 emissive { 0.0f, 0.0f, 0.0f, 1.0f };
    float  shininess = 0.0f;
    float  opacity   = 1.0f;
    bool   twoSided  = false;

    TextureRef diffuseTexture;
    TextureRef normalTexture;
    TextureRef specularTexture;
    TextureRef emissiveTexture;
};

//------------------------------------------------------------------------------
// PBR マテリアル（PBR 版 .gmdl にのみ入る追加情報）
//------------------------------------------------------------------------------

/// ランタイム側の CPbrMaterial::Params に 1:1 で対応する PBR パラメータ。
/// Model::materials と同じ添字で対応する（pbrMaterials[i] は materials[i] の PBR 版）。
/// Model::pbrMaterials が空のモデルは従来（非 PBR）フォーマット。
struct PbrMaterial {
    Float3 albedo { 1.0f, 1.0f, 1.0f };   ///< ベースカラー
    float  metallic  = 0.0f;
    float  roughness = 0.5f;
    float  ao        = 1.0f;
    Float3 f0 { 0.04f, 0.04f, 0.04f };    ///< フレネル値（f0Manual のときだけ使う）
    bool   f0Manual  = false;             ///< false = metallic から自動で決める
    float  normalStrength   = 1.0f;       ///< 法線マップの強さ
    bool   useNormalMap     = true;       ///< 法線マップを使うか（テクスチャが無ければ無視される）
    bool   flipGreenChannel = false;      ///< 緑チャンネルを反転（OpenGL 系の法線マップ）
    bool   useAlbedoTexture = true;       ///< ディフューズテクスチャを掛けるか
    bool   useMaterialColor = true;       ///< Material.Diffuse / 頂点カラーを掛けるか
};

/// モデルに埋め込まれていたテクスチャの生バイト列。
/// D3D リソース化はランタイム側の責務（このライブラリはバイト列を運ぶだけ）。
struct EmbeddedTexture {
    std::string               name;    ///< 元の参照名（"*0" など）
    std::string               format;  ///< "png" / "jpg" / "rgba8888"（非圧縮の場合）
    std::uint32_t             width  = 0;  ///< format=="rgba8888" のときのみ有効
    std::uint32_t             height = 0;  ///< 同上
    std::vector<std::uint8_t> bytes;
};

//------------------------------------------------------------------------------
// ノード階層とボーン
//------------------------------------------------------------------------------

/// シーングラフのノード 1 つ。pre-order で格納され、常に parent < 自分の添字。
struct Node {
    std::string  name;
    std::int32_t parent    = kInvalidIndex;   ///< ルートは kInvalidIndex
    Matrix4x4    localTransform{};            ///< 親からの相対（バインドポーズ）
    std::int32_t boneIndex = kInvalidIndex;   ///< 対応する Bone があれば その添字
};

/// スキニングに使われるノード。Vertex::boneIndex はこの配列を指す。
struct Bone {
    std::int32_t nodeIndex = kInvalidIndex;   ///< Model::nodes への添字
    Matrix4x4    offsetMatrix{};              ///< モデル空間 → ボーン空間（inverse bind pose）
};

//------------------------------------------------------------------------------
// アニメーション
//------------------------------------------------------------------------------

struct VectorKey { float time = 0.0f; Float3     value{}; };
struct QuatKey   { float time = 0.0f; Quaternion value{}; };

/// 1 ノードぶんのキーフレーム列。時刻はティック単位（AnimationClip 参照）。
struct NodeAnimation {
    std::int32_t           nodeIndex = kInvalidIndex;   ///< Model::nodes への添字
    std::vector<VectorKey> positionKeys;
    std::vector<QuatKey>   rotationKeys;
    std::vector<VectorKey> scaleKeys;
};

struct AnimationClip {
    std::string                name;
    float                      duration       = 0.0f;   ///< ティック単位の長さ
    float                      ticksPerSecond = 30.0f;  ///< 0 のソースは 30 で補完済み
    std::vector<NodeAnimation> channels;

    [[nodiscard]] float DurationSeconds() const noexcept {
        return ticksPerSecond > 0.0f ? duration / ticksPerSecond : 0.0f;
    }
};

//------------------------------------------------------------------------------
// モデル全体
//------------------------------------------------------------------------------

struct Model {
    std::vector<Vertex>          vertices;          ///< 全サブセット連結
    std::vector<std::uint32_t>   indices;           ///< グローバル頂点番号
    std::vector<Subset>          subsets;
    std::vector<Material>        materials;
    std::vector<PbrMaterial>     pbrMaterials;      ///< 空 = 非 PBR。非空なら materials と同数
    std::vector<EmbeddedTexture> embeddedTextures;
    std::vector<Node>            nodes;             ///< pre-order
    std::vector<Bone>            bones;
    std::vector<AnimationClip>   animations;

    // 【globalInverseTransform を持たないことについて】
    //   assimp を使った解説記事では「ルートノードの逆行列」を最後に掛ける実装が
    //   よく出てくるが、あれはメッシュをルート変換の *手前* の空間に戻すための
    //   ものであって、ルート変換をモデル行列側で別途掛ける前提の式。
    //   このフォーマットでは
    //     - 非スキンメッシュ … ルートを含むワールド変換を頂点へ焼き込み済み
    //     - スキンメッシュ  … skinMatrix = offsetMatrix * nodeWorld（ルート込み）
    //   と、どちらもルート込みの同じ空間に揃えてある。だから逆行列は要らない。
    //   ルート情報は nodes[0].localTransform に入っているので失われてもいない。

    [[nodiscard]] bool HasSkin()      const noexcept { return !bones.empty(); }
    [[nodiscard]] bool HasPbr()       const noexcept { return !pbrMaterials.empty(); }
    [[nodiscard]] bool HasAnimation() const noexcept { return !animations.empty(); }
};

//------------------------------------------------------------------------------
// ちょっとした補助（ランタイムから使うことが多いので同梱）
//------------------------------------------------------------------------------

/// 行ベクトル規約の行列積（this = a * b、a を先に適用）。
[[nodiscard]] Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b) noexcept;

/// バインドポーズのワールド行列を全ノードぶん計算する。
/// nodes が pre-order なので前から 1 パスで済む。
[[nodiscard]] std::vector<Matrix4x4> ComputeBindPoseWorldTransforms(const Model& model);

/// ノードのワールド行列（ComputeBindPoseWorldTransforms またはアニメーション
/// 評価結果）から、シェーダへ送るスキニング行列パレットを作る。
///   skinMatrix[i] = bones[i].offsetMatrix * nodeWorld[bones[i].nodeIndex]
/// Vertex::boneIndex はこの配列の添字にそのまま対応する。
[[nodiscard]] std::vector<Matrix4x4>
ComputeSkinningMatrices(const Model& model, const std::vector<Matrix4x4>& nodeWorldTransforms);

/// 名前からノード添字を引く（見つからなければ kInvalidIndex）。線形探索。
[[nodiscard]] std::int32_t FindNodeIndex(const Model& model, std::string_view name) noexcept;

/// 整合性チェック。問題があれば problems に説明を積んで false を返す。
/// コンバータの出口とローダーの入口の両方で呼んでおくと事故が減る。
[[nodiscard]] bool Validate(const Model& model, std::vector<std::string>& problems);

} // namespace gm31::model

#include "ModelBinary.h"

#include <bit>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

namespace gm31::model {
namespace {

static_assert(std::endian::native == std::endian::little,
              "gmdl はリトルエンディアン前提です。BE 対応が要るならバイトスワップを入れてください。");

// Vertex は memcpy で一括転送する。padding が入ると壊れるのでサイズを固定する。
static_assert(std::is_trivially_copyable_v<Vertex>);
static_assert(sizeof(Vertex) == 92, "Vertex のレイアウトが変わりました。kVersion を上げてください。");
static_assert(std::is_trivially_copyable_v<Matrix4x4>);
static_assert(sizeof(Matrix4x4) == 64);

//------------------------------------------------------------------------------
// チャンク ID
//------------------------------------------------------------------------------
constexpr char kChunkStringTable[4] = { 'S', 'T', 'R', 'T' };
constexpr char kChunkVertices    [4] = { 'V', 'T', 'X', 'B' };
constexpr char kChunkIndices     [4] = { 'I', 'D', 'X', 'B' };
constexpr char kChunkSubsets     [4] = { 'S', 'U', 'B', 'S' };
constexpr char kChunkMaterials   [4] = { 'M', 'T', 'R', 'L' };
constexpr char kChunkEmbeddedTex [4] = { 'E', 'T', 'E', 'X' };
constexpr char kChunkNodes       [4] = { 'N', 'O', 'D', 'E' };
constexpr char kChunkBones       [4] = { 'B', 'O', 'N', 'E' };
constexpr char kChunkAnimations  [4] = { 'A', 'N', 'I', 'M' };
constexpr char kChunkPbrMaterials[4] = { 'P', 'B', 'R', 'M' };

/// エラーメッセージ用に path を UTF-8 文字列にする。
/// path.string() は Windows では ANSI（Shift-JIS）になり ImGui で文字化けするうえ、
/// Shift-JIS で表せない文字が入っていると例外を投げるので使わない。
[[nodiscard]] std::string PathToUtf8(const std::filesystem::path& path)
{
    const std::u8string u8 = path.u8string();
    return { reinterpret_cast<const char*>(u8.data()), u8.size() };
}

/// FileHeader::flags のビット。PBR チャンク入りファイルの目印（ローダーは見なくてもよい）。
constexpr std::uint32_t kFileFlagPbrMaterials = 0x1;

/// PBRM チャンクの 1 マテリアルあたりのバイト数
///   Float3 albedo(12) + metallic(4) + roughness(4) + ao(4)
/// + Float3 f0(12) + normalStrength(4) + bool×5(5) = 45
constexpr std::size_t kPbrMaterialRecordBytes = 45;

//------------------------------------------------------------------------------
// 文字列テーブル
//------------------------------------------------------------------------------
class StringTable {
public:
    /// 文字列を登録してインデックスを返す（同じ文字列は同じインデックス）。
    std::uint32_t Add(const std::string& s)
    {
        if (const auto it = indexByValue_.find(s); it != indexByValue_.end()) {
            return it->second;
        }
        const auto index = static_cast<std::uint32_t>(values_.size());
        values_.push_back(s);
        indexByValue_.emplace(s, index);
        return index;
    }

    [[nodiscard]] const std::vector<std::string>& Values() const noexcept { return values_; }

private:
    std::vector<std::string>                        values_;
    std::unordered_map<std::string, std::uint32_t>  indexByValue_;
};

//------------------------------------------------------------------------------
// 書き込み
//------------------------------------------------------------------------------
class BinaryWriter {
public:
    explicit BinaryWriter(std::vector<std::uint8_t>& out) : out_(out) {}

    template <typename T>
    void Write(const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        const auto* first = reinterpret_cast<const std::uint8_t*>(&value);
        out_.insert(out_.end(), first, first + sizeof(T));
    }

    void WriteBytes(const void* data, std::size_t size)
    {
        if (size == 0) return;
        const auto* first = static_cast<const std::uint8_t*>(data);
        out_.insert(out_.end(), first, first + size);
    }

    /// 可変長配列は必ず「u32 の要素数」を先に書く（読み側が一様に扱えるように）
    void WriteCount(std::size_t count) { Write(static_cast<std::uint32_t>(count)); }

    template <typename T>
    void WriteArray(const std::vector<T>& values)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        WriteCount(values.size());
        WriteBytes(values.data(), values.size() * sizeof(T));
    }

private:
    std::vector<std::uint8_t>& out_;
};

/// 1 チャンクぶんのペイロードを組み立てる作業領域
struct ChunkBuffer {
    char                      id[4]{};
    std::vector<std::uint8_t> payload;
};

void WriteTextureRef(BinaryWriter& w, StringTable& strings, const TextureRef& ref)
{
    w.Write(strings.Add(ref.path));
    w.Write(ref.embeddedIndex);
}

ChunkBuffer BuildSubsetsChunk(const Model& model, StringTable& strings)
{
    ChunkBuffer chunk;
    std::memcpy(chunk.id, kChunkSubsets, 4);
    BinaryWriter w(chunk.payload);

    w.WriteCount(model.subsets.size());
    for (const Subset& s : model.subsets) {
        w.Write(strings.Add(s.name));
        w.Write(s.materialIndex);
        w.Write(s.vertexOffset);
        w.Write(s.vertexCount);
        w.Write(s.indexOffset);
        w.Write(s.indexCount);
        w.Write(s.nodeIndex);
        w.Write(static_cast<std::uint8_t>(s.hasSkin ? 1 : 0));
    }
    return chunk;
}

ChunkBuffer BuildMaterialsChunk(const Model& model, StringTable& strings)
{
    ChunkBuffer chunk;
    std::memcpy(chunk.id, kChunkMaterials, 4);
    BinaryWriter w(chunk.payload);

    w.WriteCount(model.materials.size());
    for (const Material& m : model.materials) {
        w.Write(strings.Add(m.name));
        w.Write(m.ambient);
        w.Write(m.diffuse);
        w.Write(m.specular);
        w.Write(m.emissive);
        w.Write(m.shininess);
        w.Write(m.opacity);
        w.Write(static_cast<std::uint8_t>(m.twoSided ? 1 : 0));
        WriteTextureRef(w, strings, m.diffuseTexture);
        WriteTextureRef(w, strings, m.normalTexture);
        WriteTextureRef(w, strings, m.specularTexture);
        WriteTextureRef(w, strings, m.emissiveTexture);
    }
    return chunk;
}

ChunkBuffer BuildPbrMaterialsChunk(const Model& model)
{
    ChunkBuffer chunk;
    std::memcpy(chunk.id, kChunkPbrMaterials, 4);
    BinaryWriter w(chunk.payload);

    w.WriteCount(model.pbrMaterials.size());
    for (const PbrMaterial& p : model.pbrMaterials) {
        w.Write(p.albedo);
        w.Write(p.metallic);
        w.Write(p.roughness);
        w.Write(p.ao);
        w.Write(p.f0);
        w.Write(p.normalStrength);
        w.Write(static_cast<std::uint8_t>(p.f0Manual         ? 1 : 0));
        w.Write(static_cast<std::uint8_t>(p.useNormalMap     ? 1 : 0));
        w.Write(static_cast<std::uint8_t>(p.flipGreenChannel ? 1 : 0));
        w.Write(static_cast<std::uint8_t>(p.useAlbedoTexture ? 1 : 0));
        w.Write(static_cast<std::uint8_t>(p.useMaterialColor ? 1 : 0));
    }
    return chunk;
}

ChunkBuffer BuildEmbeddedTexturesChunk(const Model& model, StringTable& strings)
{
    ChunkBuffer chunk;
    std::memcpy(chunk.id, kChunkEmbeddedTex, 4);
    BinaryWriter w(chunk.payload);

    w.WriteCount(model.embeddedTextures.size());
    for (const EmbeddedTexture& t : model.embeddedTextures) {
        w.Write(strings.Add(t.name));
        w.Write(strings.Add(t.format));
        w.Write(t.width);
        w.Write(t.height);
        w.WriteArray(t.bytes);
    }
    return chunk;
}

ChunkBuffer BuildNodesChunk(const Model& model, StringTable& strings)
{
    ChunkBuffer chunk;
    std::memcpy(chunk.id, kChunkNodes, 4);
    BinaryWriter w(chunk.payload);

    w.WriteCount(model.nodes.size());
    for (const Node& n : model.nodes) {
        w.Write(strings.Add(n.name));
        w.Write(n.parent);
        w.Write(n.localTransform);
        w.Write(n.boneIndex);
    }
    return chunk;
}

ChunkBuffer BuildBonesChunk(const Model& model)
{
    ChunkBuffer chunk;
    std::memcpy(chunk.id, kChunkBones, 4);
    BinaryWriter w(chunk.payload);

    w.WriteCount(model.bones.size());
    for (const Bone& b : model.bones) {
        w.Write(b.nodeIndex);
        w.Write(b.offsetMatrix);
    }
    return chunk;
}

ChunkBuffer BuildAnimationsChunk(const Model& model, StringTable& strings)
{
    ChunkBuffer chunk;
    std::memcpy(chunk.id, kChunkAnimations, 4);
    BinaryWriter w(chunk.payload);

    w.WriteCount(model.animations.size());
    for (const AnimationClip& clip : model.animations) {
        w.Write(strings.Add(clip.name));
        w.Write(clip.duration);
        w.Write(clip.ticksPerSecond);

        w.WriteCount(clip.channels.size());
        for (const NodeAnimation& ch : clip.channels) {
            w.Write(ch.nodeIndex);
            w.WriteArray(ch.positionKeys);
            w.WriteArray(ch.rotationKeys);
            w.WriteArray(ch.scaleKeys);
        }
    }
    return chunk;
}

//------------------------------------------------------------------------------
// 読み込み
//------------------------------------------------------------------------------
class BinaryReader {
public:
    BinaryReader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

    template <typename T>
    [[nodiscard]] T Read()
    {
        static_assert(std::is_trivially_copyable_v<T>);
        Require(sizeof(T));
        T value{};
        std::memcpy(&value, data_ + cursor_, sizeof(T));
        cursor_ += sizeof(T);
        return value;
    }

    /// 要素数を読む。壊れたファイルで巨大な reserve をしないよう上限を検査する。
    [[nodiscard]] std::size_t ReadCount(std::size_t elementSize)
    {
        const auto count = static_cast<std::size_t>(Read<std::uint32_t>());
        if (elementSize > 0 && count > (size_ - cursor_) / elementSize) {
            throw std::runtime_error("要素数がファイルサイズと矛盾しています（ファイル破損）");
        }
        return count;
    }

    template <typename T>
    [[nodiscard]] std::vector<T> ReadArray()
    {
        static_assert(std::is_trivially_copyable_v<T>);
        const std::size_t count = ReadCount(sizeof(T));
        std::vector<T>    values(count);
        if (count > 0) {
            Require(count * sizeof(T));
            std::memcpy(values.data(), data_ + cursor_, count * sizeof(T));
            cursor_ += count * sizeof(T);
        }
        return values;
    }

    void ReadBytes(void* dst, std::size_t size)
    {
        Require(size);
        std::memcpy(dst, data_ + cursor_, size);
        cursor_ += size;
    }

    [[nodiscard]] bool AtEnd() const noexcept { return cursor_ >= size_; }

private:
    void Require(std::size_t bytes) const
    {
        if (cursor_ + bytes > size_) {
            throw std::runtime_error("ファイル終端を越えて読み込もうとしました（ファイル破損）");
        }
    }

    const std::uint8_t* data_   = nullptr;
    std::size_t         size_   = 0;
    std::size_t         cursor_ = 0;
};

/// 文字列テーブル参照。範囲外インデックスは空文字列にして落とさない。
[[nodiscard]] std::string ResolveString(const std::vector<std::string>& table, std::uint32_t index)
{
    return index < table.size() ? table[index] : std::string{};
}

[[nodiscard]] TextureRef ReadTextureRef(BinaryReader& r, const std::vector<std::string>& table)
{
    TextureRef ref;
    ref.path          = ResolveString(table, r.Read<std::uint32_t>());
    ref.embeddedIndex = r.Read<std::int32_t>();
    return ref;
}

#pragma pack(push, 1)
struct FileHeader {
    char          magic[8]{};
    std::uint32_t version    = 0;
    std::uint32_t flags      = 0;
    std::uint32_t chunkCount = 0;
    std::uint32_t reserved   = 0;
};
struct ChunkHeader {
    char          id[4]{};
    std::uint64_t payloadBytes = 0;
};
#pragma pack(pop)

static_assert(sizeof(FileHeader)  == 24);
static_assert(sizeof(ChunkHeader) == 12);

} // namespace

//==============================================================================
// Serialize
//==============================================================================
std::vector<std::uint8_t> Serialize(const Model& model)
{
    StringTable strings;

    // 本体チャンクを先に組み立てる。この過程で strings が埋まるので、
    // 文字列テーブルチャンクは最後に作って先頭へ差し込む。
    std::vector<ChunkBuffer> chunks;
    chunks.push_back(BuildSubsetsChunk(model, strings));
    chunks.push_back(BuildMaterialsChunk(model, strings));
    if (!model.pbrMaterials.empty()) {
        // PBR 版のみ。従来ローダーは未知チャンクとして読み飛ばすので互換は保たれる。
        chunks.push_back(BuildPbrMaterialsChunk(model));
    }
    chunks.push_back(BuildEmbeddedTexturesChunk(model, strings));
    chunks.push_back(BuildNodesChunk(model, strings));
    chunks.push_back(BuildBonesChunk(model));
    chunks.push_back(BuildAnimationsChunk(model, strings));

    {   // 頂点・インデックスは POD 配列なので丸ごと転送
        ChunkBuffer chunk;
        std::memcpy(chunk.id, kChunkVertices, 4);
        BinaryWriter w(chunk.payload);
        w.WriteArray(model.vertices);
        chunks.push_back(std::move(chunk));
    }
    {
        ChunkBuffer chunk;
        std::memcpy(chunk.id, kChunkIndices, 4);
        BinaryWriter w(chunk.payload);
        w.WriteArray(model.indices);
        chunks.push_back(std::move(chunk));
    }
    ChunkBuffer stringChunk;
    std::memcpy(stringChunk.id, kChunkStringTable, 4);
    {
        BinaryWriter w(stringChunk.payload);
        w.WriteCount(strings.Values().size());
        for (const std::string& s : strings.Values()) {
            w.WriteCount(s.size());
            w.WriteBytes(s.data(), s.size());
        }
    }
    chunks.insert(chunks.begin(), std::move(stringChunk));

    // ---- 出力 --------------------------------------------------------------
    std::vector<std::uint8_t> out;
    std::size_t total = sizeof(FileHeader);
    for (const ChunkBuffer& c : chunks) {
        total += sizeof(ChunkHeader) + c.payload.size();
    }
    out.reserve(total);

    BinaryWriter w(out);

    FileHeader header;
    std::memcpy(header.magic, kMagic, sizeof(kMagic));
    header.version    = kVersion;
    header.flags      = model.pbrMaterials.empty() ? 0u : kFileFlagPbrMaterials;
    header.chunkCount = static_cast<std::uint32_t>(chunks.size());
    w.Write(header);

    for (const ChunkBuffer& c : chunks) {
        ChunkHeader ch;
        std::memcpy(ch.id, c.id, 4);
        ch.payloadBytes = c.payload.size();
        w.Write(ch);
        w.WriteBytes(c.payload.data(), c.payload.size());
    }
    return out;
}

//==============================================================================
// Deserialize
//==============================================================================
bool Deserialize(Model& outModel, const std::uint8_t* data, std::size_t size, std::string* error)
{
    const auto fail = [error](const std::string& message) {
        if (error != nullptr) *error = message;
        return false;
    };

    try {
        BinaryReader reader(data, size);
        const auto   header = reader.Read<FileHeader>();

        if (std::memcmp(header.magic, kMagic, sizeof(kMagic)) != 0) {
            return fail("gmdl ファイルではありません（マジックが不一致）");
        }
        if (header.version != kVersion) {
            return fail("バージョンが違います（ファイル=" + std::to_string(header.version) +
                        " / ローダー=" + std::to_string(kVersion) + "）");
        }

        Model model;

        // チャンクを一旦全部走査してペイロード位置を控える。
        // 未知の id はここで捨てられるので、将来チャンクを足しても壊れない。
        struct Span { const std::uint8_t* data; std::size_t size; };
        std::vector<std::pair<std::string, Span>> found;

        std::size_t offset = sizeof(FileHeader);
        for (std::uint32_t i = 0; i < header.chunkCount; ++i) {
            if (offset + sizeof(ChunkHeader) > size) {
                return fail("チャンクヘッダが途中で切れています");
            }
            ChunkHeader chunkHeader{};
            std::memcpy(&chunkHeader, data + offset, sizeof(ChunkHeader));
            offset += sizeof(ChunkHeader);

            if (chunkHeader.payloadBytes > size - offset) {
                return fail("チャンク長がファイルサイズを超えています");
            }
            found.emplace_back(std::string(chunkHeader.id, 4),
                               Span{ data + offset, static_cast<std::size_t>(chunkHeader.payloadBytes) });
            offset += static_cast<std::size_t>(chunkHeader.payloadBytes);
        }

        // ---- 文字列テーブルを先に復元する --------------------------------
        std::vector<std::string> stringTable;
        for (const auto& [id, span] : found) {
            if (id != std::string(kChunkStringTable, 4)) continue;
            BinaryReader r(span.data, span.size);
            const std::size_t count = r.ReadCount(sizeof(std::uint32_t));
            stringTable.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                const std::size_t length = r.ReadCount(1);
                std::string       s(length, '\0');
                r.ReadBytes(s.data(), length);
                stringTable.push_back(std::move(s));
            }
        }

        // ---- 本体 --------------------------------------------------------
        for (const auto& [id, span] : found) {
            BinaryReader r(span.data, span.size);

            if (id == std::string(kChunkVertices, 4)) {
                model.vertices = r.ReadArray<Vertex>();

            } else if (id == std::string(kChunkIndices, 4)) {
                model.indices = r.ReadArray<std::uint32_t>();

            } else if (id == std::string(kChunkSubsets, 4)) {
                const std::size_t count = r.ReadCount(29);
                model.subsets.reserve(count);
                for (std::size_t i = 0; i < count; ++i) {
                    Subset s;
                    s.name          = ResolveString(stringTable, r.Read<std::uint32_t>());
                    s.materialIndex = r.Read<std::uint32_t>();
                    s.vertexOffset  = r.Read<std::uint32_t>();
                    s.vertexCount   = r.Read<std::uint32_t>();
                    s.indexOffset   = r.Read<std::uint32_t>();
                    s.indexCount    = r.Read<std::uint32_t>();
                    s.nodeIndex     = r.Read<std::int32_t>();
                    s.hasSkin       = r.Read<std::uint8_t>() != 0;
                    model.subsets.push_back(std::move(s));
                }

            } else if (id == std::string(kChunkMaterials, 4)) {
                const std::size_t count = r.ReadCount(1);
                model.materials.reserve(count);
                for (std::size_t i = 0; i < count; ++i) {
                    Material m;
                    m.name      = ResolveString(stringTable, r.Read<std::uint32_t>());
                    m.ambient   = r.Read<Float4>();
                    m.diffuse   = r.Read<Float4>();
                    m.specular  = r.Read<Float4>();
                    m.emissive  = r.Read<Float4>();
                    m.shininess = r.Read<float>();
                    m.opacity   = r.Read<float>();
                    m.twoSided  = r.Read<std::uint8_t>() != 0;
                    m.diffuseTexture  = ReadTextureRef(r, stringTable);
                    m.normalTexture   = ReadTextureRef(r, stringTable);
                    m.specularTexture = ReadTextureRef(r, stringTable);
                    m.emissiveTexture = ReadTextureRef(r, stringTable);
                    model.materials.push_back(std::move(m));
                }

            } else if (id == std::string(kChunkEmbeddedTex, 4)) {
                const std::size_t count = r.ReadCount(1);
                model.embeddedTextures.reserve(count);
                for (std::size_t i = 0; i < count; ++i) {
                    EmbeddedTexture t;
                    t.name   = ResolveString(stringTable, r.Read<std::uint32_t>());
                    t.format = ResolveString(stringTable, r.Read<std::uint32_t>());
                    t.width  = r.Read<std::uint32_t>();
                    t.height = r.Read<std::uint32_t>();
                    t.bytes  = r.ReadArray<std::uint8_t>();
                    model.embeddedTextures.push_back(std::move(t));
                }

            } else if (id == std::string(kChunkNodes, 4)) {
                const std::size_t count = r.ReadCount(sizeof(std::uint32_t) * 2 + sizeof(Matrix4x4) + sizeof(std::int32_t));
                model.nodes.reserve(count);
                for (std::size_t i = 0; i < count; ++i) {
                    Node n;
                    n.name           = ResolveString(stringTable, r.Read<std::uint32_t>());
                    n.parent         = r.Read<std::int32_t>();
                    n.localTransform = r.Read<Matrix4x4>();
                    n.boneIndex      = r.Read<std::int32_t>();
                    model.nodes.push_back(std::move(n));
                }

            } else if (id == std::string(kChunkBones, 4)) {
                const std::size_t count = r.ReadCount(sizeof(std::int32_t) + sizeof(Matrix4x4));
                model.bones.reserve(count);
                for (std::size_t i = 0; i < count; ++i) {
                    Bone b;
                    b.nodeIndex    = r.Read<std::int32_t>();
                    b.offsetMatrix = r.Read<Matrix4x4>();
                    model.bones.push_back(b);
                }

            } else if (id == std::string(kChunkAnimations, 4)) {
                const std::size_t clipCount = r.ReadCount(1);
                model.animations.reserve(clipCount);
                for (std::size_t c = 0; c < clipCount; ++c) {
                    AnimationClip clip;
                    clip.name           = ResolveString(stringTable, r.Read<std::uint32_t>());
                    clip.duration       = r.Read<float>();
                    clip.ticksPerSecond = r.Read<float>();

                    const std::size_t channelCount = r.ReadCount(1);
                    clip.channels.reserve(channelCount);
                    for (std::size_t ch = 0; ch < channelCount; ++ch) {
                        NodeAnimation channel;
                        channel.nodeIndex    = r.Read<std::int32_t>();
                        channel.positionKeys = r.ReadArray<VectorKey>();
                        channel.rotationKeys = r.ReadArray<QuatKey>();
                        channel.scaleKeys    = r.ReadArray<VectorKey>();
                        clip.channels.push_back(std::move(channel));
                    }
                    model.animations.push_back(std::move(clip));
                }

            } else if (id == std::string(kChunkPbrMaterials, 4)) {
                const std::size_t count = r.ReadCount(kPbrMaterialRecordBytes);
                model.pbrMaterials.reserve(count);
                for (std::size_t i = 0; i < count; ++i) {
                    PbrMaterial p;
                    p.albedo           = r.Read<Float3>();
                    p.metallic         = r.Read<float>();
                    p.roughness        = r.Read<float>();
                    p.ao               = r.Read<float>();
                    p.f0               = r.Read<Float3>();
                    p.normalStrength   = r.Read<float>();
                    p.f0Manual         = r.Read<std::uint8_t>() != 0;
                    p.useNormalMap     = r.Read<std::uint8_t>() != 0;
                    p.flipGreenChannel = r.Read<std::uint8_t>() != 0;
                    p.useAlbedoTexture = r.Read<std::uint8_t>() != 0;
                    p.useMaterialColor = r.Read<std::uint8_t>() != 0;
                    model.pbrMaterials.push_back(p);
                }

            }
            // 未知のチャンクは黙って読み飛ばす（前方互換）
        }

        outModel = std::move(model);
        return true;

    } catch (const std::exception& e) {
        return fail(e.what());
    }
}

//==============================================================================
// ファイル入出力
//==============================================================================
bool WriteToFile(const Model& model, const std::filesystem::path& path, std::string* error)
{
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        if (error != nullptr) *error = "書き込み用に開けません: " + PathToUtf8(path);
        return false;
    }

    const std::vector<std::uint8_t> bytes = Serialize(model);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    if (!file) {
        if (error != nullptr) *error = "書き込みに失敗しました: " + PathToUtf8(path);
        return false;
    }
    return true;
}

bool ReadFromFile(Model& outModel, const std::filesystem::path& path, std::string* error)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        if (error != nullptr) *error = "読み込み用に開けません: " + PathToUtf8(path);
        return false;
    }

    const auto size = static_cast<std::streamsize>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (size > 0 && !file.read(reinterpret_cast<char*>(bytes.data()), size)) {
        if (error != nullptr) *error = "読み込みに失敗しました: " + PathToUtf8(path);
        return false;
    }
    return Deserialize(outModel, bytes.data(), bytes.size(), error);
}

//==============================================================================
// Describe
//==============================================================================
std::string Describe(const Model& model)
{
    std::ostringstream os;
    os << "vertices  : " << model.vertices.size() << '\n'
       << "triangles : " << model.indices.size() / 3 << '\n'
       << "subsets   : " << model.subsets.size() << '\n'
       << "materials : " << model.materials.size() << '\n'
       << "embedded  : " << model.embeddedTextures.size() << " texture(s)\n"
       << "nodes     : " << model.nodes.size() << '\n'
       << "bones     : " << model.bones.size() << '\n'
       << "animations: " << model.animations.size() << '\n'
       << "pbr       : " << (model.pbrMaterials.empty()
                             ? std::string("none")
                             : std::to_string(model.pbrMaterials.size()) + " material(s)") << '\n';

    for (std::size_t i = 0; i < model.subsets.size(); ++i) {
        const Subset& s = model.subsets[i];
        os << "  subset[" << i << "] \"" << s.name << "\""
           << " tris=" << s.indexCount / 3
           << " verts=" << s.vertexCount
           << " material=\""
           << (s.materialIndex < model.materials.size() ? model.materials[s.materialIndex].name : "?")
           << "\"" << (s.hasSkin ? " skinned" : "") << '\n';
    }
    for (std::size_t i = 0; i < model.animations.size(); ++i) {
        const AnimationClip& a = model.animations[i];
        os << "  anim[" << i << "] \"" << a.name << "\""
           << " " << a.DurationSeconds() << "s"
           << " channels=" << a.channels.size() << '\n';
    }
    return os.str();
}

} // namespace gm31::model

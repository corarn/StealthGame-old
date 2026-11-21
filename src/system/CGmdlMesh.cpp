#include	<algorithm>
#include	<chrono>
#include	<cstdio>
#include	<filesystem>
#include	<future>

#include	"CGmdlMesh.h"
#include	"gmdl/ModelBinary.h"

namespace {

	namespace fs = std::filesystem;
	namespace gmdl = gm31::model;

	/// UTF-8 の std::string から fs::path を作る。
	/// std::string を直接 fs::path に渡すと Windows では ANSI 扱いになり、
	/// 日本語を含むパス（assets/model/glinco/顔.tga など）で開けなくなる。
	fs::path Utf8ToPath(const std::string& utf8)
	{
		return fs::path(std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
	}

	Color ToColor(const gmdl::Float4& c)
	{
		return Color(c.x, c.y, c.z, c.w);
	}

	bool IsBlack(const gmdl::Float4& c)
	{
		return c.x <= 0.0f && c.y <= 0.0f && c.z <= 0.0f;
	}

	/// 埋め込みテクスチャを CTexture へ。読めなければ nullptr。
	std::unique_ptr<CTexture> LoadEmbeddedTexture(const gmdl::EmbeddedTexture& src)
	{
		// "rgba8888" は非圧縮の生ピクセル列。stb_image は解釈できないので、
		// ここでは扱わない（必要になったら ID3D11Texture2D を直接作ること）。
		if (src.format == "rgba8888" || src.bytes.empty()) {
			return nullptr;
		}

		auto texture = std::make_unique<CTexture>();
		if (!texture->LoadFromFemory(src.bytes.data(), static_cast<int>(src.bytes.size()))) {
			return nullptr;
		}
		return texture;
	}

} // namespace

bool CGmdlMesh::Load(const std::string& filename,
					 const std::string& texturedirectory,
					 const LoadOptions& options)
{
	m_lastError.clear();
	m_vertices.clear();
	m_indices.clear();
	m_materials.clear();
	m_subsets.clear();
	m_diffusetexturenames.clear();
	m_diffusetextures.clear();
	m_normaltextures.clear();
	m_model = gmdl::Model{};

	// ---- 区間計測 -----------------------------------------------------------
	// どの工程が遅いかを常に見えるようにしておく。結果は Visual Studio の
	// 「出力」ウィンドウ（デバッグ実行時）に表示される。
	using Clock = std::chrono::steady_clock;
	auto lapStart = Clock::now();
	auto lapMs = [&lapStart]() {
		const auto now = Clock::now();
		const long long ms =
			std::chrono::duration_cast<std::chrono::milliseconds>(now - lapStart).count();
		lapStart = now;
		return ms;
	};

	const fs::path modelPath = Utf8ToPath(filename);

	// ---- 読み込み ----------------------------------------------------------
	if (!gmdl::ReadFromFile(m_model, modelPath, &m_lastError)) {
		return false;
	}
	const long long msRead = lapMs();

	// 壊れたファイルをそのまま GPU へ流すと原因の分からないハングになる。
	// 描画前にここで弾いておく。
	std::vector<std::string> problems;
	if (!gmdl::Validate(m_model, problems)) {
		m_lastError = "gmdl の整合性チェックに失敗: " + problems.front();
		return false;
	}
	const long long msValidate = lapMs();

	// ---- 頂点 --------------------------------------------------------------
	m_vertices.resize(m_model.vertices.size());

	// Debug ビルドの vector::operator[] は毎回境界チェックが入り、数十万頂点の
	// ループでは無視できないコストになるので、生ポインタで回す（挙動は同じ）。
	const gmdl::Vertex* srcVertices = m_model.vertices.data();
	VERTEX_3D*          dstVertices = m_vertices.data();
	const gmdl::Bone*   boneArray   = m_model.bones.data();
	for (std::size_t i = 0, vertexNum = m_model.vertices.size(); i < vertexNum; ++i) {
		const gmdl::Vertex& src = srcVertices[i];
		VERTEX_3D&          dst = dstVertices[i];

		dst.Position = Vector3(src.position.x, src.position.y, src.position.z);
		dst.Normal   = Vector3(src.normal.x, src.normal.y, src.normal.z);
		dst.Diffuse  = Color(src.color.x, src.color.y, src.color.z, src.color.w);
		dst.TexCoord = Vector2(src.uv.x, src.uv.y);

		dst.bonecnt = 0;
		for (int b = 0; b < 4; ++b) {
			dst.BoneIndex[b]  = 0;
			dst.BoneWeight[b] = 0.0f;
//			dst.BoneName[b]   = "";
		}

		// .gmdl は「上位 4 本・合計 1.0 に正規化済み」を保証しているので、
		// ここでは詰め直すだけでよい（ウェイト 0 のスロットは使わない）。
		for (int b = 0; b < gmdl::kMaxBoneInfluence; ++b) {
			if (src.boneWeight[b] <= 0.0f) {
				continue;
			}
			const int slot = dst.bonecnt;
			dst.BoneIndex[slot]  = static_cast<int>(src.boneIndex[b]);
			dst.BoneWeight[slot] = src.boneWeight[b];

			if (options.loadBoneNames) {
				// ボーン名 = そのボーンが刺さっているノードの名前
				const std::int32_t nodeIndex = boneArray[src.boneIndex[b]].nodeIndex;
				if (nodeIndex >= 0) {
//					dst.BoneName[slot] = m_model.nodes[static_cast<std::size_t>(nodeIndex)].name;
				}
			}
			++dst.bonecnt;
		}
	}

	// ---- インデックスとサブセット ------------------------------------------
	// .gmdl のインデックスは「モデル全体で通し番号」だが、このエンジンの
	// DrawSubset() は BaseVertexLocation に SUBSET::VertexBase を渡す作りなので、
	// サブセットローカルへ直してから VertexBase を持たせる（assimp 経由の
	// CStaticMesh と同じ形にそろえる）。
	m_indices.resize(m_model.indices.size());
	m_subsets.reserve(m_model.subsets.size());

	const std::uint32_t* srcIndices = m_model.indices.data();
	std::uint32_t*       dstIndices = m_indices.data();
	for (const gmdl::Subset& src : m_model.subsets) {
		for (std::uint32_t i = 0; i < src.indexCount; ++i) {
			dstIndices[src.indexOffset + i] = srcIndices[src.indexOffset + i] - src.vertexOffset;
		}

		SUBSET dst{};
		dst.MtrlName   = (src.materialIndex < m_model.materials.size())
					   ? m_model.materials[src.materialIndex].name
					   : std::string{};
		dst.IndexNum   = src.indexCount;
		dst.VertexNum  = src.vertexCount;
		dst.IndexBase  = src.indexOffset;
		dst.VertexBase = src.vertexOffset;
		dst.MaterialIdx = src.materialIndex;
		m_subsets.emplace_back(std::move(dst));
	}

	const long long msConvert = lapMs();

	// ---- マテリアルとテクスチャ --------------------------------------------
	const fs::path textureDir = texturedirectory.empty()
							  ? modelPath.parent_path()
							  : Utf8ToPath(texturedirectory);

	m_materials.reserve(m_model.materials.size());
	m_diffusetexturenames.reserve(m_model.materials.size());
	m_diffusetextures.resize(m_model.materials.size());
	m_normaltextures.resize(m_model.materials.size());

	// ---- テクスチャの並列読み込み --------------------------------------------
	// PNG のデコード（stb_image）がモデル読み込み時間の大半を占めるので、
	// テクスチャ 1 枚 = 1 タスクとして std::async で並列に読み込む。
	//  ・stb_image は呼び出しごとに独立していてスレッド安全
	//  ・ID3D11Device のリソース生成（CreateTexture2D / CreateShaderResourceView）は
	//    フリースレッド仕様なのでワーカースレッドから呼んでよい
	//    （スレッド安全でないのは ID3D11DeviceContext のほう。ここでは使わない）
	auto loadTextureTask = [this, textureDir](gmdl::TextureRef ref) -> std::unique_ptr<CTexture> {
		if (ref.IsEmbedded()) {
			const auto embeddedIndex = static_cast<std::size_t>(ref.embeddedIndex);
			if (embeddedIndex < m_model.embeddedTextures.size()) {
				return LoadEmbeddedTexture(m_model.embeddedTextures[embeddedIndex]);
			}
			return nullptr;
		}
		const fs::path full = textureDir / Utf8ToPath(ref.path);
		auto texture = std::make_unique<CTexture>();
		if (!texture->Load(full.u8string())) {
			return nullptr;
		}
		return texture;
	};

	std::vector<std::future<std::unique_ptr<CTexture>>> diffuseFutures(m_model.materials.size());
	std::vector<std::future<std::unique_ptr<CTexture>>> normalFutures(m_model.materials.size());

	if (options.loadTextures) {
		for (std::size_t i = 0; i < m_model.materials.size(); ++i) {
			const gmdl::Material& src = m_model.materials[i];
			if (src.diffuseTexture.IsValid()) {
				diffuseFutures[i] = std::async(std::launch::async, loadTextureTask, src.diffuseTexture);
			}
			if (src.normalTexture.IsValid()) {
				normalFutures[i] = std::async(std::launch::async, loadTextureTask, src.normalTexture);
			}
		}
	}

	for (std::size_t i = 0; i < m_model.materials.size(); ++i) {
		const gmdl::Material& src = m_model.materials[i];

		MATERIAL dst{};
		dst.Ambient  = ToColor(src.ambient);
		dst.Diffuse  = ToColor(src.diffuse);
		dst.Specular = ToColor(src.specular);
		dst.Emission = ToColor(src.emissive);
		dst.Shiness  = src.shininess;

		// アンビエントが真っ黒だと、ライトの当たらない面が完全な黒に潰れる。
		// 「読み込めているのに何も見えない」の大半がこれなので既定で補正する。
		if (options.useDiffuseAsAmbientFallback && IsBlack(src.ambient)) {
			dst.Ambient = Color(src.diffuse.x * options.ambientFallbackRate,
								src.diffuse.y * options.ambientFallbackRate,
								src.diffuse.z * options.ambientFallbackRate,
								1.0f);
		}

		const gmdl::TextureRef& tex = src.diffuseTexture;
		dst.TextureEnable = FALSE;
		m_diffusetexturenames.emplace_back(tex.path);

		// 並列タスクの結果を受け取る（get() はそのテクスチャの完了まで待つ）。
		// 読めなかったテクスチャは TextureEnable を立てないでおく。
		// ここで立ててしまうと、描画時に null の CTexture を触って落ちる。
		if (diffuseFutures[i].valid()) {
			if (auto texture = diffuseFutures[i].get()) {
				m_diffusetextures[i] = std::move(texture);
				dst.TextureEnable    = TRUE;
			}
		}

		// ---- 法線マップ（PBR パスの t4 用）----------------------------------
		// 法線マップは「色」ではなく「向き」なので sRGB 変換をしてはいけない。
		// CTexture は UNORM で SRV を作るのでそのまま読めば正しい。
		// 読めなくても失敗にはしない（シェーダ側で useNormalMap が落ちるだけ）。
		if (normalFutures[i].valid()) {
			if (auto texture = normalFutures[i].get()) {
				m_normaltextures[i] = std::move(texture);
			}
		}

		m_materials.emplace_back(dst);
	}
	const long long msTexture = lapMs();

	// ---- 後始末 ------------------------------------------------------------
	// VERTEX_3D へ移し替えたので、gmdl 側の頂点／インデックスは捨てる。
	// （残しておくと大きいモデルでメモリを二重に食う）
	// ノード階層・ボーン・アニメーションは GetModel() で使うので残す。
	m_model.vertices.clear();
	m_model.vertices.shrink_to_fit();
	m_model.indices.clear();
	m_model.indices.shrink_to_fit();
	m_model.embeddedTextures.clear();
	m_model.embeddedTextures.shrink_to_fit();

	// ---- 計測結果を出力 ------------------------------------------------------
	{
		char log[512];
		std::snprintf(log, sizeof(log),
			"[CGmdlMesh::Load] %s : read=%lldms validate=%lldms convert=%lldms texture=%lldms\n",
			filename.c_str(), msRead, msValidate, msConvert, msTexture);
		OutputDebugStringA(log);
	}

	return true;
}

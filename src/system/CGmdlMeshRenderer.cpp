#include	<algorithm>
#include	<cmath>

#include	"CGmdlMeshRenderer.h"

void CGmdlMeshRenderer::Init(CGmdlMesh& mesh)
{
	// 頂点バッファとインデックスバッファを生成
	CMeshRenderer::Init(mesh);

	m_subsets         = mesh.GetSubsets();
	m_diffuseTextures = mesh.GetDiffuseTextures();		// 所有権を受け取る（破壊的）
	m_normalTextures  = mesh.GetNormalTextures();		// 同上（PBR の t4 用）

	// マテリアルごとにコンスタントバッファを用意する
	const std::vector<MATERIAL>& materials = mesh.GetMaterials();
	m_materials.reserve(materials.size());
	for (const MATERIAL& m : materials) {
		auto material = std::make_unique<CMaterial>();
		material->Create(m);
		m_materials.emplace_back(std::move(material));
	}

	// ---- PBR マテリアル（b8）を用意する -------------------------------------
	// PBR 版 .gmdl（PBRM チャンク入り）ならその値をそのまま使い、
	// 無ければ従来マテリアルから近似値を作る。どちらでも DrawPbr() で描ける。
	const gm31::model::Model& model = mesh.GetModel();
	m_hasPbrData = !materials.empty()
				&& model.pbrMaterials.size() == materials.size();

	m_pbrMaterials.reserve(materials.size());
	for (std::size_t i = 0; i < materials.size(); ++i) {
		auto pbr = std::make_unique<CPbrMaterial>();
		pbr->Init();

		CPbrMaterial::Params params{};
		if (m_hasPbrData) {
			// コンバータが PBRM チャンクへ書いた値（CPbrMaterial::Params と 1:1）
			const gm31::model::PbrMaterial& src = model.pbrMaterials[i];
			params.albedo           = { src.albedo.x, src.albedo.y, src.albedo.z };
			params.metallic         = src.metallic;
			params.roughness        = src.roughness;
			params.ao               = src.ao;
			params.f0               = { src.f0.x, src.f0.y, src.f0.z };
			params.f0Manual         = src.f0Manual;
			params.normalStrength   = src.normalStrength;
			params.useNormalMap     = src.useNormalMap;
			params.flipGreenChannel = src.flipGreenChannel;
			params.useAlbedoTexture = src.useAlbedoTexture;
			params.useMaterialColor = src.useMaterialColor;
		} else {
			// 従来マテリアルからの近似。色は Material.Diffuse（b3）側が持つので
			// albedo は白のまま、roughness だけ shininess から作る
			// （コンバータの AssimpImporter と同じ Beckmann の変換式）。
			const MATERIAL& m = materials[i];
			params.roughness = (m.Shiness > 0.0f)
				? std::clamp(std::sqrt(2.0f / (m.Shiness + 2.0f)), 0.045f, 1.0f)
				: 0.6f;
		}

		// 法線マップは「テクスチャが実際に読めた」ときだけ有効にする
		params.useNormalMap = params.useNormalMap
							&& i < m_normalTextures.size()
							&& m_normalTextures[i] != nullptr;

		pbr->SetParams(params);
		m_pbrMaterials.emplace_back(std::move(pbr));
	}
}

void CGmdlMeshRenderer::Draw()
{
	// インデックスバッファ・頂点バッファをセット
	BeforeDraw();

	for (int i = 0; i < static_cast<int>(m_subsets.size()); ++i) {
		if (m_visibleSubset >= 0 && m_visibleSubset < static_cast<int>(m_subsets.size())
			&& i != m_visibleSubset) {
			continue;
		}

		const SUBSET& subset = m_subsets[i];
		if (subset.IndexNum == 0) {
			continue;
		}

		// マテリアルインデックスは Validate 済みだが、
		// 手で組んだ Model を渡される可能性もあるのでここでも見ておく。
		if (subset.MaterialIdx >= m_materials.size()) {
			continue;
		}

		CMaterial* material = m_materials[subset.MaterialIdx].get();
		material->SetGPU();

		// テクスチャは「使う設定になっていて、かつ実際に読めていた」ときだけセットする。
		// CStaticMeshRenderer はここを見ていないので、テクスチャファイルが
		// 見つからないモデルを読ませると null 参照で落ちる。
		if (material->isDiffuseTextureEnable()
			&& subset.MaterialIdx < m_diffuseTextures.size()
			&& m_diffuseTextures[subset.MaterialIdx]) {
			m_diffuseTextures[subset.MaterialIdx]->SetGPU();
		}

		DrawSubset(
			subset.IndexNum,		// 描画するインデックス数
			subset.IndexBase,		// 最初のインデックスバッファの位置
			subset.VertexBase);		// 頂点バッファのベース位置
	}
}

//------------------------------------------------------------------------------
// PBR 用の描画。Draw() と同じ回し方で、b8（PBR マテリアル）と t4（法線マップ）を
// サブセット毎に追加セットする。シェーダ（pbrVS / pbrPS）と IBL のリソース
// （CIblEnvironment::Bind）は呼び出し側でセットしておくこと。
//------------------------------------------------------------------------------
void CGmdlMeshRenderer::DrawPbr()
{
	// インデックスバッファ・頂点バッファをセット
	BeforeDraw();

	for (int i = 0; i < static_cast<int>(m_subsets.size()); ++i) {
		if (m_visibleSubset >= 0 && m_visibleSubset < static_cast<int>(m_subsets.size())
			&& i != m_visibleSubset) {
			continue;
		}

		const SUBSET& subset = m_subsets[i];
		if (subset.IndexNum == 0) {
			continue;
		}
		if (subset.MaterialIdx >= m_materials.size()) {
			continue;
		}

		// b3: pbrPS も Material.Diffuse / TextureEnable / Emission を参照する
		CMaterial* material = m_materials[subset.MaterialIdx].get();
		material->SetGPU();

		// b8: PBR マテリアル
		if (subset.MaterialIdx < m_pbrMaterials.size() && m_pbrMaterials[subset.MaterialIdx]) {
			m_pbrMaterials[subset.MaterialIdx]->Bind();
		}

		// t0: ディフューズ（アルベド）テクスチャ。ガードは Draw() と同じ。
		if (material->isDiffuseTextureEnable()
			&& subset.MaterialIdx < m_diffuseTextures.size()
			&& m_diffuseTextures[subset.MaterialIdx]) {
			m_diffuseTextures[subset.MaterialIdx]->SetGPU();
		}

		// t4: 法線マップ（読めていれば）
		if (subset.MaterialIdx < m_normalTextures.size() && m_normalTextures[subset.MaterialIdx]) {
			m_normalTextures[subset.MaterialIdx]->SetGPU(4);
		}

		DrawSubset(
			subset.IndexNum,
			subset.IndexBase,
			subset.VertexBase);
	}
}

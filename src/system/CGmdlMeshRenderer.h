#pragma once
//==============================================================================
// CGmdlMeshRenderer.h
//
//  CGmdlMesh 用のレンダラ。中身は CStaticMeshRenderer とほぼ同じだが、
//  テクスチャが読めなかったサブセットで落ちないようにガードを足してある。
//==============================================================================

#include	<memory>
#include	<vector>

#include	"CGmdlMesh.h"
#include	"CMaterial.h"
#include	"CMeshRenderer.h"
#include	"CPbrMaterial.h"
#include	"CTexture.h"

class CGmdlMeshRenderer : public CMeshRenderer {
public:
	void Init(CGmdlMesh& mesh);

	/// サブセットを順に描く。ワールド行列とシェーダは呼び出し側でセットしておくこと。
	void Draw() override;

	/// PBR 用の描画。pbrVS / pbrPS をセットし、CIblEnvironment::Bind() を
	/// 呼んでから使うこと。従来の Draw() との違いは、サブセット毎に
	///   - PBR マテリアル（b8）
	///   - 法線マップ（t4）
	/// を追加でセットする点だけ。
	void DrawPbr();

	/// .gmdl に PBRM チャンク（PBR マテリアル）が入っていたか。
	/// false のときは従来マテリアルから作った近似値で描いている。
	bool HasPbrData() const { return m_hasPbrData; }

	int GetPbrMaterialCount() const { return static_cast<int>(m_pbrMaterials.size()); }

	/// ImGui からパラメータをいじる用。範囲外は nullptr。
	CPbrMaterial* GetPbrMaterial(int index) {
		return (index >= 0 && index < static_cast<int>(m_pbrMaterials.size()))
			 ? m_pbrMaterials[index].get() : nullptr;
	}

	/// 描画するサブセットを絞る（ImGui でパーツ確認をするとき用）。
	/// 範囲外を渡すと全サブセットを描く。
	void SetVisibleSubset(int index) { m_visibleSubset = index; }
	int  GetSubsetCount() const { return static_cast<int>(m_subsets.size()); }
	const std::vector<SUBSET>& GetSubsets() const { return m_subsets; }

private:
	std::vector<SUBSET>						m_subsets;
	std::vector<std::unique_ptr<CTexture>>	m_diffuseTextures;
	std::vector<std::unique_ptr<CTexture>>	m_normalTextures;		// PBR の t4 用
	std::vector<std::unique_ptr<CMaterial>>	m_materials;
	std::vector<std::unique_ptr<CPbrMaterial>>	m_pbrMaterials;		// PBR の b8 用
	bool									m_hasPbrData = false;	// PBRM チャンク入りだったか
	int										m_visibleSubset = -1;   // -1 で全部
};

#pragma once
//==============================================================================
// CGmdlMesh.h
//
//  自前フォーマット .gmdl を読み込んで、エンジンの CMesh（VERTEX_3D / SUBSET /
//  MATERIAL）へ変換するクラス。CStaticMesh の .gmdl 版という位置づけで、
//  インターフェースはわざと CStaticMesh に揃えてある。
//
//  CStaticMesh との違い
//    - assimp を使わない。読み込みは system/gmdl/ModelBinary.h だけで完結する。
//    - ノード階層・ボーン・アニメーションも保持する（GetModel() で参照できる）。
//
//  使い方
//    auto mesh = std::make_unique<CGmdlMesh>();
//    mesh->Load("assets/model/obj/player.gmdl");     // テクスチャは同じ階層から探す
//    auto renderer = std::make_unique<CGmdlMeshRenderer>();
//    renderer->Init(*mesh);
//    ...
//    renderer->Draw();
//==============================================================================

#include	<memory>
#include	<string>
#include	<vector>

#include	"CMesh.h"
#include	"CTexture.h"
#include	"renderer.h"
#include	"gmdl/ModelFormat.h"

/// CGmdlMesh::Load のオプション。
/// クラス内の入れ子にすると、既定メンバ初期化子を持つ型を同じクラスの
/// 既定引数に書けない（不完全型扱いになる）ため、名前空間スコープに置いてある。
struct GmdlLoadOptions {
	/// VERTEX_3D::BoneName を埋める。スキニングをボーン名で引く既存コードと
	/// 揃えたい場合は true。静的表示しかしないなら false にすると
	/// 頂点あたり std::string 4 個ぶんのメモリを節約できる。
	bool loadBoneNames = true;

	/// ディフューズテクスチャを実際に読み込む（false ならファイル名だけ保持）。
	bool loadTextures = true;

	/// マテリアルのアンビエントが真っ黒なとき、ディフューズ×係数で補う。
	/// 多くのエクスポータはアンビエントを 0 で書き出すため、
	/// そのままだと陰の側が完全な黒に潰れて「読めたのに見えない」ことになる。
	bool  useDiffuseAsAmbientFallback = true;
	float ambientFallbackRate = 0.25f;
};

class CGmdlMesh : public CMesh {
public:
	using LoadOptions = GmdlLoadOptions;

	/// .gmdl を読み込む。
	/// @param filename        .gmdl のパス（UTF-8）
	/// @param texturedirectory テクスチャを探すフォルダ。空なら .gmdl と同じ階層。
	/// @return 成功したら true。失敗理由は GetLastError()。
	bool Load(const std::string& filename,
			  const std::string& texturedirectory = "",
			  const LoadOptions& options = GmdlLoadOptions{});

	// ---- CStaticMesh と同じインターフェース --------------------------------

	const std::vector<MATERIAL>& GetMaterials() const { return m_materials; }

	const std::vector<SUBSET>& GetSubsets() const { return m_subsets; }

	const std::vector<std::string>& GetDiffuseTextureNames() const { return m_diffusetexturenames; }

	/// テクスチャ群の所有権を渡す（CStaticMesh と同じく破壊的。1 回しか呼べない）
	std::vector<std::unique_ptr<CTexture>> GetDiffuseTextures() { return std::move(m_diffusetextures); }

	/// 法線マップ群の所有権を渡す（同上。PBR パスの t4 用）
	std::vector<std::unique_ptr<CTexture>> GetNormalTextures() { return std::move(m_normaltextures); }

	// ---- .gmdl 固有 ---------------------------------------------------------

	/// ノード階層・ボーン・アニメーションを参照する。
	/// メモリ節約のため、頂点／インデックスは VERTEX_3D へ変換した時点で
	/// 捨ててある（二重に持たない）。形状は GetVertices() / GetIndices() を使うこと。
	const gm31::model::Model& GetModel() const { return m_model; }

	const std::string& GetLastError() const { return m_lastError; }

private:
	std::vector<MATERIAL>					m_materials;			// マテリアル情報
	std::vector<std::string>				m_diffusetexturenames;	// ディフューズテクスチャ名
	std::vector<SUBSET>						m_subsets;				// サブセット情報
	std::vector<std::unique_ptr<CTexture>>	m_diffusetextures;		// ディフューズテクスチャ群
	std::vector<std::unique_ptr<CTexture>>	m_normaltextures;		// 法線マップ群（PBR 用）

	gm31::model::Model						m_model;				// ノード／ボーン／アニメーション
	std::string								m_lastError;
};

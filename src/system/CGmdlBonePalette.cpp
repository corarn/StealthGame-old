//==============================================================================
// CGmdlBonePalette.cpp
//==============================================================================
#include	<chrono>

#include	"CGmdlBonePalette.h"

//------------------------------------------------------------------------------
void CGmdlBonePalette::Clear()
{
	m_palette.clear();
	m_boneCount    = 0;
	m_overflow     = false;
	m_lastUpdateMs = 0.0f;
	// m_buffer（定数バッファ）は作り直さない。
	// D3D リソースの生成は安くないうえ、中身は Update() で毎回上書きされるので、
	// モデルを差し替えても使い回して問題ない。
}

//------------------------------------------------------------------------------
bool CGmdlBonePalette::Init(const CGmdlSkeleton& skeleton)
{
	Clear();

	m_boneCount = skeleton.GetBoneCount();
	if (m_boneCount <= 0) {
		return false;								// ボーンの無い静的モデル
	}

	// ★ 黙って切り捨てない。
	//   定数バッファに収まらないぶんを無視すると、そのボーンに紐づく頂点だけが
	//   原点へ飛ぶ。「なぜか一部だけ壊れる」という一番追いにくい壊れ方になる。
	if (m_boneCount > MAX_BONE) {
		m_overflow = true;
		return false;
	}

	if (!m_created) {
		if (!m_buffer.Create()) {
			return false;
		}
		m_created = true;
	}

	m_palette.assign(static_cast<std::size_t>(m_boneCount), Matrix4x4::Identity);
	return true;
}

//------------------------------------------------------------------------------
void CGmdlBonePalette::Update(const CGmdlSkeleton& skeleton)
{
	if (!IsUsable()) {
		return;
	}

	const auto begin = std::chrono::high_resolution_clock::now();

	// CPU 版とまったく同じ関数を呼ぶ。
	// 「GPU 版にしたら行列の作り方も変わる」ということは無い。
	// この土台では MakeSkinMatrix() の名前が BuildSkinningPalette() で、作った配列を返す形になっている。
	// 毎フレーム配列を作り直すが、骨 66 本ぶん（約 4KB）なので気にしない。
	m_palette = skeleton.BuildSkinningPalette();

	constexpr std::size_t kMaxBone = static_cast<std::size_t>(MAX_BONE);
	const std::size_t count = (m_palette.size() < kMaxBone) ? m_palette.size() : kMaxBone;
	for (std::size_t i = 0; i < count; ++i) {
		// ★ Transpose を忘れるとモデルが潰れる（ヘッダの解説を参照）
		m_buffer.ConstantBufferMemory.BoneCombMtx[i] = m_palette[i].Transpose();
	}

	// 使っていない残りは単位行列にしておく。
	// 前のモデルの行列が残っていると、ボーン数の少ないモデルに差し替えたとき
	// 範囲外を引いた頂点が前のポーズへ飛ぶ。
	for (std::size_t i = count; i < kMaxBone; ++i) {
		m_buffer.ConstantBufferMemory.BoneCombMtx[i] = Matrix4x4::Identity;
	}

	m_buffer.Update();

	const auto end = std::chrono::high_resolution_clock::now();
	m_lastUpdateMs = std::chrono::duration<float, std::milli>(end - begin).count();
}

//------------------------------------------------------------------------------
void CGmdlBonePalette::SetGPU()
{
	if (!IsUsable()) {
		return;
	}
	m_buffer.SetGPU();								// b5 レジスタへ
}

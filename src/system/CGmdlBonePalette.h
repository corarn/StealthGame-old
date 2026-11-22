#pragma once
//==============================================================================
// CGmdlBonePalette.h
//
//  GPU スキニング用に、ボーン行列パレットを定数バッファ（b5）へ載せるクラス。
//  CPU 版（CGmdlSkinner）と対になる「GPU 側の担当」。
//
//  ■ CPU 版と GPU 版で何が違うのか
//      違うのは「どこで頂点を動かすか」だけで、行列の作り方は完全に同じ。
//      どちらも CGmdlSkeleton::MakeSkinMatrix() の結果をそのまま使う。
//
//        CPU 版 : 行列で頂点を動かして、頂点バッファごと GPU へ送る
//                 → 転送量 = 頂点数 × sizeof(VERTEX_3D)   （数 MB）
//        GPU 版 : 行列だけ GPU へ送って、頂点シェーダに動かしてもらう
//                 → 転送量 = sizeof(CBBoneCombMatrix)      （25.6 KB 固定）
//
//      halchan1（29,012 頂点）で比べると 6 MB 対 25.6 KB。約 240 倍の差になる。
//      しかも CPU 版は「頂点数に比例」、GPU 版は「ボーン数に依らず固定」。
//      アニメーション再生に入って毎フレーム更新になった瞬間、この差が効いてくる。
//
//  ■ 転置して送ることについて  ★ ハマりどころ
//      HLSL の matrix は既定で列優先（column_major）で packing される。
//      一方 C++ 側の Matrix4x4 は行優先。そのまま送ると
//      シェーダの中では転置された行列として読まれてしまう。
//
//      このエンジンは「送る直前に Transpose() する」という方針で統一していて、
//      Renderer::SetWorldMatrix() も CAnimationMesh も全部そうしている。
//      2 回転置されるので、結果としてシェーダの中では C++ と同じ行列に見える。
//      ここだけ転置を忘れると、モデルが原点付近でぐしゃぐしゃに潰れる。
//
//  ■ MAX_BONE = 400 の制限
//      定数バッファのサイズは固定なので、ボーン数が 400 を超えるモデルは
//      そのままでは載らない。黙って壊すのが一番まずいので、
//      Init() で検出して IsOverflow() を立て、呼び出し側が CPU 版へ
//      退避できるようにしてある。
//      （手持ちのモデルは最大 244 本 = jack2 なので、今のところ全部収まる）
//
//  ■ 使い方
//      m_bonePalette.Init(m_skeleton);      // モデル読み込み直後
//      m_bonePalette.Update(m_skeleton);    // ポーズを変えたら
//      ...
//      m_bonePalette.SetGPU();              // 描画直前（b5 へセット）
//==============================================================================

#include	<vector>

#include	"BoneCombMatrix.h"
#include	"CGmdlSkeleton.h"
#include	"commontypes.h"
#include	"renderer.h"

class CGmdlBonePalette {
public:
	/// 定数バッファを用意し、この骨格が GPU スキニングで扱えるかを判定する。
	/// @return 使えるなら true（＝ボーン有り・400 本以下・バッファ生成成功）
	bool Init(const CGmdlSkeleton& skeleton);

	void Clear();

	/// GPU スキニングに使えるか（ボーンが 1 本以上あり、400 本を超えていない）
	[[nodiscard]] bool IsUsable() const noexcept { return m_created && m_boneCount > 0 && !m_overflow; }

	/// ボーン数が MAX_BONE を超えていたか（超えたら CPU 版へ退避すること）
	[[nodiscard]] bool IsOverflow() const noexcept { return m_overflow; }

	[[nodiscard]] int GetBoneCount() const noexcept { return m_boneCount; }
	[[nodiscard]] static int GetMaxBone() noexcept { return MAX_BONE; }

	/// 定数バッファ 1 回ぶんの転送バイト数（CPU 版と比べるため）
	[[nodiscard]] static int GetTransferBytes() noexcept { return static_cast<int>(sizeof(CBBoneCombMatrix)); }

	/// 今のポーズでパレットを作り直し、定数バッファへ転送する。
	/// ポーズを変えたときだけ呼べばよい。
	void Update(const CGmdlSkeleton& skeleton);

	/// 定数バッファを b5 レジスタへセットする（描画直前）
	void SetGPU();

	/// 直近の Update() にかかった時間【ミリ秒】
	[[nodiscard]] float GetLastUpdateMilliseconds() const noexcept { return m_lastUpdateMs; }

private:
	BoneCombMatrix			m_buffer;			///< 定数バッファ本体（NonCopyable）
	std::vector<Matrix4x4>	m_palette;			///< 作業用。確保は 1 回だけ

	int   m_boneCount    = 0;
	bool  m_created      = false;
	bool  m_overflow     = false;
	float m_lastUpdateMs = 0.0f;
};

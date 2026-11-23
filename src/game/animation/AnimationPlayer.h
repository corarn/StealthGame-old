#pragma once
#include <string>
#include "BonePose.h"

class CGmdlSkeleton;

namespace gmdl 
{
	struct Model;
}

// .gmdl に入っているアニメを1つ選んで流し、今の時間の姿勢を骨に入れる係
// モデルとアニメが同じ .gmdl に入っている前提（アニメの骨の番号が、そのまま骨格の番号になる）。別のファイルのアニメは使えない
class AnimationPlayer
{
private:
	// アニメのデータ。持ち主は Player の m_mesh で、ここは借りているだけ（Player が両方持つので、こちらより先に消えない）
	const gmdl::Model* m_model = nullptr;

	// アニメ1本ぶんの「どれを・どこまで流したか」
	struct ClipState
	{
		int   m_clipIndex = 0;
		float m_timeSec   = 0.0f;
	};

	ClipState m_current = ClipState();

	// 切り替えた瞬間に画面に出ていた姿勢。ここから今のアニメへ混ぜる。撮った後は動かない
	// 前のアニメを流し続けずに止めるのは、混ぜている途中でまた切り替えても、画面の姿勢から始めて飛ばないようにするため
	Pose m_fromPose = Pose();

	// 切り替えてからの時間。切り替えにかける時間（.cpp の BLEND_DURATION_SEC）で止まる
	float m_blendElapsedSec = 0.0f;

	// 撮った姿勢と今のアニメを、切り替えてからの時間で混ぜた姿勢（＝今、画面に出す姿勢）
	Pose CalculateDisplayedPose() const;

public:
	explicit AnimationPlayer(const gmdl::Model& model);

	// 名前でアニメを選び、最初から流す。今と同じアニメなら何もしない（毎フレーム呼んでも最初に戻らない）
	// 見つからなければ false を返して、今のアニメのまま
	bool Play(const std::string& clipName);

	// 時間を進める。最後まで来たら最初に戻る（ループ）
	void Update(float deltaTimeSec);

	// 今の時間の姿勢を骨格に入れて、骨格の Update() まで呼ぶ
	void ApplyTo(CGmdlSkeleton& skeleton) const;
};

#pragma once
#include <string>

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

	int   m_clipIndex = 0;
	float m_timeSec   = 0.0f;

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

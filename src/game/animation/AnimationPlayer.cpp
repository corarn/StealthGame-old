#include "AnimationPlayer.h"
#include <cmath>
#include <cstddef>
#include <vector>
#include "../../system/CGmdlSkeleton.h"
#include "../../system/commontypes.h"
#include "../../system/gmdl/ModelFormat.h"

namespace
{
	// ticks のすぐ前のキーの番号を返す（キーの時刻が 0, 10, 20, 30 で ticks = 25 なら 2）
	// 最後のキーより後ろなら、最後のキーの番号を返す。keys は空でないこと
	std::size_t FindKeyIndex(const std::vector<gmdl::VectorKey>& keys, float ticks)
	{
		std::size_t index = 0;
		while (index + 1 < keys.size() && keys[index + 1].time <= ticks)
		{
			++index;
		}
		return index;
	}

	// 回転のキー用。キーの型が違うだけで、中身は上と同じ
	// TODO: テンプレートを習ったら、2つを1つにまとめる
	std::size_t FindKeyIndex(const std::vector<gmdl::QuatKey>& keys, float ticks)
	{
		std::size_t index = 0;
		while (index + 1 < keys.size() && keys[index + 1].time <= ticks)
		{
			++index;
		}
		return index;
	}

	// startTicks から endTicks のあいだで、ticks がどこにいるかを 0.0〜1.0 で返す（20, 30, 25 なら 0.5）
	// キーの時刻は必ず増えていく（同じ時刻のキーは無い）ので、0 で割ることはない
	float CalculateRatio(float startTicks, float endTicks, float ticks)
	{
		return (ticks - startTicks) / (endTicks - startTicks);
	}

	// 位置か大きさのキーの列から、ticks の時点の値を作る
	Vector3 Sample(const std::vector<gmdl::VectorKey>& keys, float ticks)
	{
		std::size_t index = FindKeyIndex(keys, ticks);
		const gmdl::VectorKey& current = keys[index];
		Vector3 currentValue = Vector3(current.value.x, current.value.y, current.value.z);

		// 最後のキーまで来ていたら、混ぜる相手がいないのでそのまま返す
		if (index + 1 == keys.size())
		{
			return currentValue;
		}

		const gmdl::VectorKey& next = keys[index + 1];
		Vector3 nextValue = Vector3(next.value.x, next.value.y, next.value.z);
		float ratio = CalculateRatio(current.time, next.time, ticks);
		return Vector3::Lerp(currentValue, nextValue, ratio);
	}

	// 回転のキーの列から、ticks の時点の回転を作る
	Quaternion Sample(const std::vector<gmdl::QuatKey>& keys, float ticks)
	{
		std::size_t index = FindKeyIndex(keys, ticks);
		const gmdl::QuatKey& current = keys[index];
		Quaternion currentValue = Quaternion(current.value.x, current.value.y, current.value.z, current.value.w);

		// 最後のキーまで来ていたら、混ぜる相手がいないのでそのまま返す
		if (index + 1 == keys.size())
		{
			return currentValue;
		}

		const gmdl::QuatKey& next = keys[index + 1];
		Quaternion nextValue = Quaternion(next.value.x, next.value.y, next.value.z, next.value.w);
		float ratio = CalculateRatio(current.time, next.time, ticks);

		// 回転は Lerp で混ぜると、途中で長さが 1 でなくなり、体が縮んだりゆがんだりする。Slerp は球の表面に沿って混ぜる
		return Quaternion::Slerp(currentValue, nextValue, ratio);
	}
}

AnimationPlayer::AnimationPlayer(const gmdl::Model& model)
	: m_model(&model)
{
}

bool AnimationPlayer::Play(const std::string& clipName)
{
	for (int i = 0; i < static_cast<int>(m_model->animations.size()); ++i)
	{
		if (m_model->animations[i].name == clipName)
		{
			if (i != m_clipIndex)
			{
				m_clipIndex = i;
				m_timeSec   = 0.0f;
			}
			return true;
		}
	}
	return false;
}

void AnimationPlayer::Update(float deltaTimeSec)
{
	if (m_model->animations.empty())
	{
		return;
	}

	const gmdl::AnimationClip& clip = m_model->animations[m_clipIndex];
	float durationSec = clip.DurationSeconds();
	if (durationSec <= 0.0f)
	{
		return;
	}

	m_timeSec += deltaTimeSec;
	if (m_timeSec >= durationSec)
	{
		// 0 に戻すと、超えたぶんの時間が消えて動きが少し飛ぶ。超えたぶんだけ残して最初に戻す
		m_timeSec = std::fmod(m_timeSec, durationSec);
	}
}

void AnimationPlayer::ApplyTo(CGmdlSkeleton& skeleton) const
{
	if (m_model->animations.empty())
	{
		return;
	}

	const gmdl::AnimationClip& clip = m_model->animations[m_clipIndex];

	// キーの時刻は秒ではなく「ティック」（clip.ticksPerSecond 個で 1 秒）
	float ticks = m_timeSec * clip.ticksPerSecond;

	for (const gmdl::NodeAnimation& channel : clip.channels)
	{
		// 3 つそろっていない骨は、ファイルの T ポーズのまま（player.glb は全部そろっている）
		if (channel.positionKeys.empty() || channel.rotationKeys.empty() || channel.scaleKeys.empty())
		{
			continue;
		}

		Vector3    position = Sample(channel.positionKeys, ticks);
		Quaternion rotation = Sample(channel.rotationKeys, ticks);
		Vector3    scale    = Sample(channel.scaleKeys, ticks);

		// 大きさ → 回転 → 位置 の順に掛ける。逆にすると、骨が自分の根元ではなく親の根元を中心に回ってしまう
		Matrix4x4 local = Matrix4x4::CreateScale(scale) * Matrix4x4::CreateFromQuaternion(rotation) * Matrix4x4::CreateTranslation(position);
		skeleton.SetNodeLocal(channel.nodeIndex, local);
	}

	skeleton.Update();
}

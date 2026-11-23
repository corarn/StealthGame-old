#include "AnimationPlayer.h"
#include "BonePose.h"
#include <cmath>
#include <cstddef>
#include <vector>
#include "../../system/CGmdlSkeleton.h"
#include "../../system/commontypes.h"
#include "../../system/gmdl/ModelFormat.h"

namespace
{
	// TODO: ステップ3で、切り替えてからの時間で 0 → 1 に変える。今は混ぜ方を確かめるための決め打ち
	constexpr float TEMPORARY_BLEND_WEIGHT = 0.5f;

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

	// clip の timeSec 秒の姿勢を、ノードの数ぶんの表にして返す
	Pose SamplePose(const gmdl::AnimationClip& clip, float timeSec, int nodeCount)
	{
		Pose pose = Pose(static_cast<std::size_t>(nodeCount));

		// キーの時刻は秒ではなく「ティック」（clip.ticksPerSecond 個で 1 秒）
		float ticks = timeSec * clip.ticksPerSecond;

		for (const gmdl::NodeAnimation& channel : clip.channels)
		{
			// 3 つそろっていない骨は、ファイルの T ポーズのまま（player.glb は全部そろっている）
			if (channel.positionKeys.empty() || channel.rotationKeys.empty() || channel.scaleKeys.empty())
			{
				continue;
			}

			BonePose& bonePose = pose[channel.nodeIndex];
			bonePose.m_position   = Sample(channel.positionKeys, ticks);
			bonePose.m_rotation   = Sample(channel.rotationKeys, ticks);
			bonePose.m_scale      = Sample(channel.scaleKeys, ticks);
			bonePose.m_isAnimated = true;
		}
		return pose;
	}

	// 表の姿勢を骨格に入れる。m_isAnimated が false のノードは、ファイルの T ポーズのまま
	void ApplyPose(const Pose& pose, CGmdlSkeleton& skeleton)
	{
		for (int i = 0; i < static_cast<int>(pose.size()); ++i)
		{
			const BonePose& bonePose = pose[i];
			if (!bonePose.m_isAnimated)
			{
				continue;
			}

			// 大きさ → 回転 → 位置 の順に掛ける。逆にすると、骨が自分の根元ではなく親の根元を中心に回ってしまう
			Matrix4x4 local = Matrix4x4::CreateScale(bonePose.m_scale) * Matrix4x4::CreateFromQuaternion(bonePose.m_rotation) * Matrix4x4::CreateTranslation(bonePose.m_position);
			skeleton.SetNodeLocal(i, local);
		}
	}

	// from と to を weight で混ぜる（0 なら from のまま、1 なら to のまま）
	// 片方にしかアニメがない骨は、ある方をそのまま使う。from と to は同じモデルの表なので、長さは同じ
	Pose BlendPoses(const Pose& from, const Pose& to, float weight)
	{
		Pose result = Pose(from.size());
		for (std::size_t i = 0; i < from.size(); ++i)
		{
			if (!from[i].m_isAnimated)
			{
				result[i] = to[i];
				continue;
			}
			if (!to[i].m_isAnimated)
			{
				result[i] = from[i];
				continue;
			}

			result[i].m_position   = Vector3::Lerp(from[i].m_position, to[i].m_position, weight);
			result[i].m_rotation   = Quaternion::Slerp(from[i].m_rotation, to[i].m_rotation, weight);
			result[i].m_scale      = Vector3::Lerp(from[i].m_scale, to[i].m_scale, weight);
			result[i].m_isAnimated = true;
		}
		return result;
	}

	// timeSec を deltaTimeSec だけ進める。最後まで来たら最初に戻す（ループ）
	float AdvanceTime(const gmdl::AnimationClip& clip, float timeSec, float deltaTimeSec)
	{
		float durationSec = clip.DurationSeconds();
		if (durationSec <= 0.0f)
		{
			return timeSec;
		}

		float nextTimeSec = timeSec + deltaTimeSec;
		if (nextTimeSec >= durationSec)
		{
			// 0 に戻すと、超えたぶんの時間が消えて動きが少し飛ぶ。超えたぶんだけ残して最初に戻す
			nextTimeSec = std::fmod(nextTimeSec, durationSec);
		}
		return nextTimeSec;
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
			if (i != m_current.m_clipIndex)
			{
				// 前のアニメは止めずに流し続けて、今のアニメと混ぜる
				m_previous = m_current;
				m_current.m_clipIndex = i;
				m_current.m_timeSec   = 0.0f;
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

	// Idle と Walk は長さが違うので、前と今で別々に時間を進める
	m_current.m_timeSec  = AdvanceTime(m_model->animations[m_current.m_clipIndex], m_current.m_timeSec, deltaTimeSec);
	m_previous.m_timeSec = AdvanceTime(m_model->animations[m_previous.m_clipIndex], m_previous.m_timeSec, deltaTimeSec);
}

void AnimationPlayer::ApplyTo(CGmdlSkeleton& skeleton) const
{
	if (m_model->animations.empty())
	{
		return;
	}

	int  nodeCount    = skeleton.GetNodeCount();
	Pose previousPose = SamplePose(m_model->animations[m_previous.m_clipIndex], m_previous.m_timeSec, nodeCount);
	Pose currentPose  = SamplePose(m_model->animations[m_current.m_clipIndex], m_current.m_timeSec, nodeCount);
	Pose pose         = BlendPoses(previousPose, currentPose, TEMPORARY_BLEND_WEIGHT);
	ApplyPose(pose, skeleton);
	skeleton.Update();
}
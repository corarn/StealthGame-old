#pragma once
#include <vector>
#include "../../system/commontypes.h"

// 骨1本の、ある時間の姿勢。行列ではなく3つに分けて持つ（行列のまま混ぜると、腕や足が縮むから）
struct BonePose
{
	Vector3    m_position   = Vector3(0.0f, 0.0f, 0.0f);
	Quaternion m_rotation   = Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
	Vector3    m_scale      = Vector3(1.0f, 1.0f, 1.0f);
	bool       m_isAnimated = false;
};

// 番号はノードの番号（channel.nodeIndex）と同じ。アニメのないノードは m_isAnimated が false のまま
using Pose = std::vector<BonePose>;

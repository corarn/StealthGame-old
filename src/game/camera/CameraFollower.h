#pragma once
#include "../../system/commontypes.h"

class Camera;

// シーンのカメラを外から動かして、追う点（プレイヤー）の後ろの斜め上に置く係。
// カメラ本体は追従のことを知らない。追う点はシーンが毎フレーム渡すので、この係も Character を知らない
class CameraFollower
{
private:
	Camera* m_camera = nullptr;  // 持ち主はシーン。ここは住所だけ

public:
	explicit CameraFollower(Camera& camera);

	// targetPosition は追う物の足元の位置
	void Update(const Vector3& targetPosition);
};

#include "CameraFollower.h"
#include "../../system/camera.h"

namespace
{
	// 追う点から見たカメラの位置。後ろ（Z+ の側）に 5.6m・上に 3m。前の仮のカメラと同じ見え方にした
	// TODO: ステップ4でマウスで回すようにしたら、角度と距離に置きかえる
	constexpr Vector3 CAMERA_OFFSET = Vector3(0.0f, 3.0f, 5.6f);

	// 足元ではなく腰のあたりを見る。足元を見ると、プレイヤーが画面の上に寄るから
	constexpr float LOOKAT_HEIGHT = 1.0f;
}

CameraFollower::CameraFollower(Camera& camera)
	: m_camera(&camera)
{
}

void CameraFollower::Update(const Vector3& targetPosition)
{
	Vector3 lookat = targetPosition + Vector3(0.0f, LOOKAT_HEIGHT, 0.0f);
	m_camera->SetPosition(targetPosition + CAMERA_OFFSET);
	m_camera->SetLookat(lookat);
}

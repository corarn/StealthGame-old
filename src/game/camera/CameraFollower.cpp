#include "CameraFollower.h"
#include <cmath>
#include "../../system/camera.h"

namespace
{
	constexpr Vector3 CAMERA_OFFSET = Vector3(0.0f, 3.0f, 5.6f);

	constexpr float LOOKAT_HEIGHT = 1.0f;

	constexpr float FOLLOW_SHARPNESS_PER_SEC = 8.0f;

	constexpr float MICROSEC_PER_SEC = 1000000.0f;
}

CameraFollower::CameraFollower(Camera& camera)
	: m_camera(&camera)
{
}

void CameraFollower::SnapTo(const Vector3& targetPosition)
{
	m_followPosition = targetPosition;
	ApplyToCamera();
}

void CameraFollower::Update(const Vector3& targetPosition, uint64_t deltaTimeMicrosec)
{
	float deltaTimeSec = static_cast<float>(deltaTimeMicrosec) / MICROSEC_PER_SEC;

	float followRate = 1.0f - std::exp(-FOLLOW_SHARPNESS_PER_SEC * deltaTimeSec);
	m_followPosition += (targetPosition - m_followPosition) * followRate;
	ApplyToCamera();
}

void CameraFollower::ApplyToCamera()
{
	Vector3 lookat = m_followPosition + Vector3(0.0f, LOOKAT_HEIGHT, 0.0f);
	m_camera->SetPosition(m_followPosition + CAMERA_OFFSET);
	m_camera->SetLookat(lookat);
}

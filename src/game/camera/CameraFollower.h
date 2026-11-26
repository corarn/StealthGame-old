#pragma once
#include <cstdint>
#include "../../system/commontypes.h"

class Camera;

class CameraFollower
{
private:
	Camera* m_camera = nullptr;

	Vector3 m_followPosition = Vector3(0.0f, 0.0f, 0.0f);

	void ApplyToCamera();

public:
	explicit CameraFollower(Camera& camera);

	void SnapTo(const Vector3& targetPosition);

	void Update(const Vector3& targetPosition, uint64_t deltaTimeMicrosec);
};

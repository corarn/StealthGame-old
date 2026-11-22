#include "FreeCameraDebugPanel.h"
#include <cmath>
#include "../../system/camera.h"
#include "../../system/CPolar3D.h"
#include "../../system/DebugUI.h"
#include "../../system/imgui/imgui.h"

namespace
{
	constexpr float MIN_RADIUS = 1.0f;
	constexpr float MAX_RADIUS = 1000.0f;

	constexpr float MIN_ELEVATION_DEG = 1.0f;   // 0度と180度やったら、
	constexpr float MAX_ELEVATION_DEG = 179.0f; // 真上・真下で画面がひっくり返るから

	constexpr float MIN_AZIMUTH_DEG = -180.0f;
	constexpr float MAX_AZIMUTH_DEG = 180.0f;

	constexpr float LOOKAT_RANGE = 200.0f;  // マップは原点から約 160m 先まであるから、それより少し広く
}

FreeCameraDebugPanel::FreeCameraDebugPanel(Camera* camera)
	: m_camera(camera)
{
	m_lookat = m_camera->GetLookat();

	// CPolor3D の逆の計算。カメラの位置と見る点が同じだと r が 0 になって割り算が壊れる（今は起きない）
	Vector3 offset = m_camera->GetPosition() - m_lookat;
	m_radius       = offset.Length();
	m_elevationRad = std::acos(offset.y / m_radius);
	m_azimuthRad   = std::atan2(offset.z, offset.x);
}

void FreeCameraDebugPanel::RegisterToDebugUI()
{
	DebugUI::RedistDebugFunction([this]()
		{
			DrawPanel();
		});
}

void FreeCameraDebugPanel::DrawPanel()
{
	if (ImGui::CollapsingHeader("Free Camera"))
	{
		ImGui::SliderFloat ("Radius",    &m_radius,       MIN_RADIUS,        MAX_RADIUS);
		ImGui::SliderAngle ("Elevation", &m_elevationRad, MIN_ELEVATION_DEG, MAX_ELEVATION_DEG);
		ImGui::SliderAngle ("Azimuth",   &m_azimuthRad,   MIN_AZIMUTH_DEG,   MAX_AZIMUTH_DEG);
		ImGui::SliderFloat ("Lookat X",  &m_lookat.x,     -LOOKAT_RANGE,     LOOKAT_RANGE);
		ImGui::SliderFloat ("Lookat Y",  &m_lookat.y,     -LOOKAT_RANGE,     LOOKAT_RANGE);
		ImGui::SliderFloat ("Lookat Z",  &m_lookat.z,     -LOOKAT_RANGE,     LOOKAT_RANGE);

		ApplyToCamera();  // パネルを閉じてるときはカメラを触らない
	}
}

void FreeCameraDebugPanel::ApplyToCamera()
{
	CPolor3D polar = CPolor3D(m_radius, m_elevationRad, m_azimuthRad);
	m_camera->SetPosition(polar.ToCartesian() + m_lookat);
	m_camera->SetLookat(m_lookat);
}

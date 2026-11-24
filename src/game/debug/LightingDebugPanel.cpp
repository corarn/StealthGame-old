#include "LightingDebugPanel.h"
#include <algorithm>
#include "../../system/CEnvironmentCube.h"
#include "../../system/DebugUI.h"
#include "../../system/renderer.h"
#include "../../system/imgui/imgui.h"

namespace
{
	// 先生の GP23 と同じ幅。暗い倉庫で足りなければ広げる
	constexpr float MAX_INTENSITY   = 5.0f;
	constexpr float MIN_EXPOSURE_EV = -4.0f;  // 1EV で明るさ2倍。-4 で 1/16
	constexpr float MAX_EXPOSURE_EV = 4.0f;

	// 向きが全部 0 近くだと、長さ1にするときに 0 で割ってしまう。そのときは前の向きのままにする
	constexpr float MIN_DIRECTION_LENGTH = 0.0001f;
}

LightingDebugPanel::LightingDebugPanel(CEnvironmentCube& environment)
	: m_environment(&environment)
{
	LIGHT light = Renderer::GetLight();
	m_isLightEnabled = (light.Enable != FALSE);
	m_direction      = Vector3(light.Direction.x, light.Direction.y, light.Direction.z);

	// 例：Diffuse が (1.5, 1.5, 1.5) なら、強さ 1.5・色 (1, 1, 1)。真っ黒（強さ 0）なら色は白のまま
	m_intensity = std::max({ light.Diffuse.x, light.Diffuse.y, light.Diffuse.z });
	if (m_intensity > 0.0f)
	{
		m_color = Color(light.Diffuse.x / m_intensity, light.Diffuse.y / m_intensity, light.Diffuse.z / m_intensity, 1.0f);
	}

	m_ambient = light.Ambient.x;  // 環境光は灰色（r = g = b）で使う
}

void LightingDebugPanel::RegisterToDebugUI()
{
	DebugUI::RedistDebugFunction([this]()
		{
			DrawPanel();
		});
}

void LightingDebugPanel::DrawPanel()
{
	if (ImGui::CollapsingHeader("Lighting"))
	{
		ImGui::Checkbox   ("Light enabled", &m_isLightEnabled);
		ImGui::SliderFloat("Direction X",   &m_direction.x, -1.0f, 1.0f);
		ImGui::SliderFloat("Direction Y",   &m_direction.y, -1.0f, 1.0f);
		ImGui::SliderFloat("Direction Z",   &m_direction.z, -1.0f, 1.0f);
		ImGui::ColorEdit3 ("Light color",   &m_color.x);
		ImGui::SliderFloat("Intensity",     &m_intensity, 0.0f, MAX_INTENSITY);
		ImGui::SliderFloat("Ambient",       &m_ambient,   0.0f, 1.0f);
		ImGui::SliderFloat("Exposure EV",   &m_environment->EditSettings().exposure, MIN_EXPOSURE_EV, MAX_EXPOSURE_EV);

		ApplyToRenderer();  // 欄を閉じているときは光を触らない
	}
}

void LightingDebugPanel::ApplyToRenderer()
{
	LIGHT light = Renderer::GetLight();
	light.Enable = m_isLightEnabled ? TRUE : FALSE;

	if (m_direction.Length() > MIN_DIRECTION_LENGTH)
	{
		Vector3 direction = m_direction;
		direction.Normalize();  // シェーダは長さ1の向きとして使う
		light.Direction = Vector4(direction.x, direction.y, direction.z, 0.0f);
	}

	light.Diffuse = Color(m_color.x * m_intensity, m_color.y * m_intensity, m_color.z * m_intensity, 1.0f);
	light.Ambient = Color(m_ambient, m_ambient, m_ambient, 1.0f);
	Renderer::SetLight(light);
}

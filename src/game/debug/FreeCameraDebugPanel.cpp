#include "FreeCameraDebugPanel.h"
#include "../../system/camera.h"
#include "../../system/DebugUI.h"
#include "../../system/imgui/imgui.h"

FreeCameraDebugPanel::FreeCameraDebugPanel(Camera* camera)
	: m_camera(camera)
{
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
	}
}

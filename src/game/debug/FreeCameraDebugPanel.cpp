#include "FreeCameraDebugWindow.h"
#include "../../system/camera.h"
#include "../../system/DebugUI.h"
#include "../../system/imgui/imgui.h"

FreeCameraDebugWindow::FreeCameraDebugWindow(Camera* camera)
	: m_camera(camera)
{
}

void FreeCameraDebugWindow::RegisterToDebugUI()
{
	DebugUI::RedistDebugFunction([this]()
	{
		DrawWindow();
	});
}

void FreeCameraDebugWindow::DrawWindow()
{
	ImGui::Begin("Free Camera");
	ImGui::End();
}

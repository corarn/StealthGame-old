#pragma once

class Camera;

class FreeCameraDebugWindow
{
private:
	Camera* m_camera = nullptr;

	void DrawWindow();

public:
	explicit FreeCameraDebugWindow(Camera* camera);

	void RegisterToDebugUI();
};

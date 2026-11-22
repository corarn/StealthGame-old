#pragma once

class Camera;

class FreeCameraDebugPanel
{
private:
	Camera* m_camera = nullptr;

	void DrawPanel();

public:
	explicit FreeCameraDebugPanel(Camera* camera);

	void RegisterToDebugUI();
};

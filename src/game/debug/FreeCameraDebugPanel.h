#pragma once
#include "../../system/commontypes.h"

class Camera;

// デバッグ用。スライダーでカメラをぐるっと回して、マップを好きな向きから見る欄
class FreeCameraDebugPanel
{
private:
	Camera* m_camera = nullptr;  // 持ち主はシーン。ここは住所だけ

	float   m_radius       = 0.0f;
	float   m_elevationRad = 0.0f;  // 真上から何度倒れてるか
	float   m_azimuthRad   = 0.0f;
	Vector3 m_lookat       = Vector3(0.0f, 0.0f, 0.0f);

	void DrawPanel();
	void ApplyToCamera();

public:
	// カメラの今の位置から、距離と角度を逆算しておく（欄を開いた瞬間にカメラが飛ばないように）
	// camera は位置を決めてから渡す（決める前に渡すと、逆算がずれる）
	explicit FreeCameraDebugPanel(Camera& camera);

	// TODO: DebugUI は登録を消せない。この欄が消えたあとも呼ばれて落ちるので、シーンを切り替えるようになったら直す
	void RegisterToDebugUI();
};

#pragma once
#include "../../system/commontypes.h"

class CEnvironmentCube;

// デバッグ用。光（Renderer が持つ平行光源1本）と露出をスライダーで動かして、倉庫の暗さを決める欄
// 決まった値は GameScene の定数に写す
class LightingDebugPanel
{
private:
	CEnvironmentCube* m_environment = nullptr;  // 持ち主はシーン。ここは住所だけ

	// 土台の LIGHT は「色 × 強さ」をかけた値しか持たないので、欄の中だけ色と強さに分けて持つ
	bool    m_isLightEnabled = true;
	Vector3 m_direction      = Vector3(0.0f, -1.0f, 0.0f);  // 光が進む向き（来る向きではない）
	Color   m_color          = Color(1.0f, 1.0f, 1.0f, 1.0f);
	float   m_intensity      = 1.0f;
	float   m_ambient        = 0.0f;

	void DrawPanel();
	void ApplyToRenderer();

public:
	// Renderer の今の光から、色と強さを逆算しておく（欄を開いた瞬間に光が変わらないように）
	explicit LightingDebugPanel(CEnvironmentCube& environment);

	// TODO: DebugUI は登録を消せない。シーンを切り替えるようになったら直す（FreeCameraDebugPanel と同じ）
	void RegisterToDebugUI();
};

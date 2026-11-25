#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include "../../system/IScene.h"
#include "../../system/SceneClassFactory.h"
#include "../../system/C3DShape.h"
#include "../../gameobject/GameObject.h"
#include "../debug/FreeCameraDebugPanel.h"
#include "../debug/LightingDebugPanel.h"
#include "../../system/CEnvironmentCube.h"
#include "../camera/CameraFollower.h"

class Character;

class GameScene : public SceneBase
{
private:
	static constexpr std::size_t AXIS_COUNT = 3;  // X・Y・Z

	static constexpr std::array<Vector3, AXIS_COUNT> AXIS_DIRECTIONS =
	{
		Vector3(1.0f, 0.0f, 0.0f),  // X
		Vector3(0.0f, 1.0f, 0.0f),  // Y
		Vector3(0.0f, 0.0f, 1.0f)   // Z
	};
	static constexpr std::array<Color, AXIS_COUNT> AXIS_COLORS =
	{
		Color(1.0f, 0.0f, 0.0f),  // 赤
		Color(0.0f, 1.0f, 0.0f),  // 緑
		Color(0.0f, 0.0f, 1.0f)   // 青
	};

	// 軸の線1本の持ち主。Segment は空で作れないので init() で作る
	using AxisLine = std::unique_ptr<Segment>;

	// 原点に出す X・Y・Z の軸の線。マップが見えないとき、読み込みとカメラのどっちが悪いか見分ける用
	std::array<AxisLine, AXIS_COUNT> m_axisLines = {};

	// 画面にカメラのデバッグ表示
	std::unique_ptr<FreeCameraDebugPanel> m_freeCameraPanel = nullptr;

	// PBR のシェーダに、カメラの位置と露出を送る係。露出は画面全体の明るさなので、物ではなくシーンが持つ
	// 作るだけなら GPU を触らない（定数バッファは Init() で作る）ので、unique_ptr にせず直接持つ
	CEnvironmentCube m_environment = CEnvironmentCube();

	// 画面に光と露出のデバッグ表示
	std::unique_ptr<LightingDebugPanel> m_lightingPanel = nullptr;

	// カメラをプレイヤーに追わせる係
	std::unique_ptr<CameraFollower> m_cameraFollower = nullptr;

	// 持ち主はシーンの物のリスト。カメラに毎フレーム位置を渡すために住所だけ覚える
	Character* m_player = nullptr;

public:
	void Update(uint64_t deltaTimeMicrosec) override;
	void Draw(uint64_t deltaTimeMicrosec) override;
	void Init() override;
	void Dispose() override;
};

REGISTER_CLASS(GameScene)
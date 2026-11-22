#include "GameScene.h"
#include "../../system/LineDrawer.h"

namespace
{
	// 仮のカメラ位置。マップ全体（約 320m 四方）が見える高さ。追従カメラを作ったら消す
	constexpr Vector3 CAMERA_POSITION = Vector3(0.0f, 200.0f, -250.0f);
	constexpr Vector3 CAMERA_LOOKAT   = Vector3(0.0f,   0.0f,    0.0f);
	constexpr Vector3 CAMERA_UP       = Vector3(0.0f,   1.0f,    0.0f);

	// 軸の線の長さ。カメラが遠いので長めにしてる
	constexpr float AXIS_LENGTH = 20.0f;
}

void GameScene::Init()
{
	// 親（SceneBase）の m_camera を使ってる。敵とかが getcamera() でカメラを聞けるように
	m_camera = std::make_unique<Camera>(CAMERA_POSITION, CAMERA_LOOKAT, CAMERA_UP);
	m_freeCameraPanel = std::make_unique<FreeCameraDebugPanel>(m_camera.get());
	m_freeCameraPanel->RegisterToDebugUI();

	// Segment を描く前に1回呼ぶ（線用のシェーダーをここで読み込む）
	LineDrawerInit();

	for (std::size_t i = 0; i < AXIS_COUNT; ++i)
	{
		m_axisLines[i] = std::make_unique<Segment>(Vector3::Zero, AXIS_DIRECTIONS[i] * AXIS_LENGTH);
	}
}

void GameScene::Update(uint64_t deltaTimeMicrosec)
{
}

void GameScene::Draw(uint64_t deltaTimeMicrosec)
{
	// 3D のものを描くより先に呼ぶ
	// ビュー行列とプロジェクション行列を GPU に送る
	m_camera->Draw();

	for (std::size_t i = 0; i < AXIS_COUNT; ++i)
	{
		m_axisLines[i]->Draw(/* world = */ Matrix4x4::Identity, AXIS_COLORS[i]);
	}
}

void GameScene::Dispose()
{
}
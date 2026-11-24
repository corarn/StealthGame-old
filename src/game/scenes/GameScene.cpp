#include "GameScene.h"
#include "../../system/LineDrawer.h"
#include "../stage/Stage.h"
#include "../player/Player.h"
#include "../../system/renderer.h"
#include <iostream>

namespace
{
	// 仮のカメラ位置。プレイヤーを正面（Z+ の側）の上から見る。後ろ（Z- の側）は壁で見えないため。追従カメラを作ったら消す
	constexpr Vector3 CAMERA_POSITION = Vector3(-41.0f, 3.0f, -25.4f);
	constexpr Vector3 CAMERA_LOOKAT   = Vector3(-41.0f, 1.0f, -31.0f);
	constexpr Vector3 CAMERA_UP       = Vector3(  0.0f, 1.0f,   0.0f);

	// 軸の線の長さ。カメラが遠いので長めにしてる
	constexpr float AXIS_LENGTH = 20.0f;

	// stageの光。LightingDebugPanel で画面を見ながら決めたとりあえず仮の数値。斜め上から当てて、壁の面ごとに明るさの差をつける
	constexpr Vector3 LIGHT_DIRECTION = Vector3(0.364f, -0.594f, 0.582f);
	constexpr Color   LIGHT_COLOR     = Color  (  1.0f,    1.0f,   1.0f, 1.0f);
	constexpr float   LIGHT_INTENSITY = 1.774f;
	constexpr float   AMBIENT         = 0.383f;
	constexpr float   EXPOSURE_EV     = 1.954f;

	// プレイヤーを最初に置く場所
	constexpr Vector3 PLAYER_SPAWN_POSITION = Vector3(-41.0f, 0.0f, -31.0f);
}

void GameScene::Init()
{
	// 親（SceneBase）の m_camera を使ってる。敵とかが GetCamera() でカメラを聞けるように
	m_camera = std::make_unique<Camera>(CAMERA_POSITION, CAMERA_LOOKAT, CAMERA_UP);
	m_freeCameraPanel = std::make_unique<FreeCameraDebugPanel>(*m_camera);
	m_freeCameraPanel->RegisterToDebugUI();

	if (!m_environment.Init())
	{
		std::cout << "GameScene: CEnvironmentCube の定数バッファを作れなかった" << std::endl;
	}

	// 欄は今の光から値を読むので、欄を作るより先に倉庫の光と露出を入れる
	Vector3 lightDirection = LIGHT_DIRECTION;
	lightDirection.Normalize();
	LIGHT light = Renderer::GetLight();
	light.Enable    = TRUE;
	light.Direction = Vector4(lightDirection.x, lightDirection.y, lightDirection.z, 0.0f);
	light.Diffuse   = Color(LIGHT_COLOR.x * LIGHT_INTENSITY, LIGHT_COLOR.y * LIGHT_INTENSITY, LIGHT_COLOR.z * LIGHT_INTENSITY, 1.0f);
	light.Ambient   = Color(AMBIENT, AMBIENT, AMBIENT, 1.0f);
	Renderer::SetLight(light);
	m_environment.EditSettings().exposure = EXPOSURE_EV;

	m_lightingPanel = std::make_unique<LightingDebugPanel>(m_environment);
	m_lightingPanel->RegisterToDebugUI();

	AddGameObject<Stage>(/* layer = */ 0);

	Player* player = AddGameObject<Player>(/* layer = */ 0);
	player->SetPosition(PLAYER_SPAWN_POSITION);

	// Segment を描く前に1回呼ぶ（線用のシェーダーをここで読み込む）
	LineDrawerInit();

	for (std::size_t i = 0; i < AXIS_COUNT; ++i)
	{
		m_axisLines[i] = std::make_unique<Segment>(Vector3::Zero, AXIS_DIRECTIONS[i] * AXIS_LENGTH);
	}
}

void GameScene::Update(uint64_t deltaTimeMicrosec)
{
	UpdateGameObjects(deltaTimeMicrosec);
}

void GameScene::Draw(uint64_t deltaTimeMicrosec)
{
	// 3D のものを描くより先に呼ぶ
	// ビュー行列とプロジェクション行列を GPU に送る
	m_camera->Draw();

	// PBR で描く物より先に送る。カメラはフリーカメラで動くので、動いたあとの位置を毎フレーム送る
	m_environment.Update(*m_camera);
	m_environment.Bind();

	DrawGameObjects(deltaTimeMicrosec);

	for (std::size_t i = 0; i < AXIS_COUNT; ++i)
	{
		m_axisLines[i]->Draw(/* world = */ Matrix4x4::Identity, AXIS_COLORS[i]);
	}
}

void GameScene::Dispose()
{
}
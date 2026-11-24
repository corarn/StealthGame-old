#include "Player.h"
#include <cmath>
#include <iostream>
#include "../../system/CShader.h"
#include "../../system/meshmanager.h"
#include "../../system/renderer.h"
#include "PlayerController.h"

namespace
{
	constexpr const char* MODEL_PATH = "assets/model/player/playerPBR.gmdl";

	// 骨の行列で頂点を動かす PBR のシェーダ。動かない Stage の "Pbr" とは別にする
	constexpr const char* SHADER_KEY = "PbrSkin";

	// 土台から来る時間はマイクロ秒。アニメは秒で進めるので直す
	constexpr float MICROSEC_PER_SEC = 1000000.0f;

	// player.glb に入っているアニメの名前（Mixamo から持ってきたときの名前）
	constexpr const char* IDLE_CLIP_NAME = "Idle";
	constexpr const char* WALK_CLIP_NAME = "Walk";

	// 人が歩く速さくらい（1 秒に 1.4m）。足がすべって見えたら、Walk のアニメに合わせて変える
	constexpr float MOVE_SPEED_PER_SEC = 1.4f;
}

Player::Player(SceneBase* scene)
	: GameObject(scene)
{
}

void Player::Init()
{
	// TODO: 体を Character に作り変えたら、頭をどこで作るかを決め直す（敵の体には敵の頭を付けたいので）
	m_controller = std::make_unique<PlayerController>(*m_ownerscene);

	if (!ShaderManager::Contains(SHADER_KEY))
	{
		std::unique_ptr<CShader> shader = std::make_unique<CShader>();
		shader->Create("shader/pbrOneSkinVS.hlsl", "shader/pbrPS.hlsl");
		ShaderManager::Register<CShader>(SHADER_KEY, std::move(shader));
	}

	std::unique_ptr<CGmdlMesh> mesh = std::make_unique<CGmdlMesh>();
	if (!mesh->Load(MODEL_PATH))
	{
		std::cout << "Player: " << MODEL_PATH << " を読めなかった: " << mesh->GetLastError() << std::endl;
		return;
	}

	m_renderer = std::make_unique<CGmdlMeshRenderer>();
	m_renderer->Init(*mesh);

	m_skeleton = std::make_unique<CGmdlSkeleton>();
	m_skeleton->Build(mesh->GetModel());

	m_animationPlayer = std::make_unique<AnimationPlayer>(mesh->GetModel());
	if (!m_animationPlayer->Play(IDLE_CLIP_NAME))
	{
		std::cout << "Player: アニメ " << IDLE_CLIP_NAME << " が見つからなかった" << std::endl;
	}

	m_bonePalette = std::make_unique<CGmdlBonePalette>();
	if (!m_bonePalette->Init(*m_skeleton))
	{
		std::cout << "Player: 骨の行列を GPU に送る準備ができなかった（骨の数 " << m_skeleton->GetBoneCount() << "）" << std::endl;
	}

	// 送る入れ物は最初は全部 0 の行列。Update より先に Draw が来ると体が原点に潰れるので、先に一度詰めておく
	m_bonePalette->Update(*m_skeleton);

	m_mesh = std::move(mesh);
}

void Player::Update(uint64_t deltaTimeMicrosec)
{
	if (!m_bonePalette)
	{
		return;
	}

	float deltaTimeSec = static_cast<float>(deltaTimeMicrosec) / MICROSEC_PER_SEC;

	Vector3 moveDirection = m_controller->DecideMoveDirection();
	m_srt.pos += moveDirection * MOVE_SPEED_PER_SEC * deltaTimeSec;

	// Play は今と同じアニメなら何もしないので、毎フレーム呼んでも最初に戻らない
	if (moveDirection.LengthSquared() > 0.0f)
	{
		m_animationPlayer->Play(WALK_CLIP_NAME);

		// モデルの前は +Z なので、+Z から進む向きまで何ラジアン回るかを atan2(x, z) で出す。
		// 止まったときに入れると atan2(0, 0) = 0 で毎回 +Z を向いてしまうので、動いているときだけ変える
		float yawRad = std::atan2(moveDirection.x, moveDirection.z);
		m_srt.rot.y = yawRad;
	}
	else
	{
		m_animationPlayer->Play(IDLE_CLIP_NAME);
	}

	m_animationPlayer->Update(deltaTimeSec);

	// 骨にアニメの姿勢を入れてから、骨の行列を作って送る（逆だと 1 フレーム前の姿勢が送られる）
	m_animationPlayer->ApplyTo(*m_skeleton);
	m_bonePalette->Update(*m_skeleton);
}

void Player::Draw(uint64_t deltaTimeMicrosec)
{
	if (!m_renderer)
	{
		return;
	}

	ShaderManager::Get<CShader>(SHADER_KEY)->SetGPU();

	Matrix4x4 world = m_srt.GetMatrix();
	Renderer::SetWorldMatrix(&world);

	m_bonePalette->SetGPU();
	m_renderer->DrawPbr();
}

void Player::Dispose()
{
}

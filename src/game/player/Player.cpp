#include "Player.h"
#include <iostream>
#include "../../system/CShader.h"
#include "../../system/meshmanager.h"
#include "../../system/renderer.h"

namespace
{
	constexpr const char* MODEL_PATH = "assets/model/player/playerPBR.gmdl";

	// 骨の行列で頂点を動かす PBR のシェーダ。動かない Stage の "Pbr" とは別にする
	constexpr const char* SHADER_KEY = "PbrSkin";
}

Player::Player(SceneBase* scene)
	: GameObject(scene)
{
}

void Player::Init()
{
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

	// 骨はまだ T ポーズのまま。崩れずに T ポーズが出れば、骨の行列の送り方が正しい
	m_renderer->DrawPbr();
}

void Player::Dispose()
{
}

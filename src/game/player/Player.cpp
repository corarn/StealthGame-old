#include "Player.h"
#include <iostream>
#include "../../system/CShader.h"
#include "../../system/meshmanager.h"
#include "../../system/renderer.h"

namespace
{
	constexpr const char* MODEL_PATH = "assets/model/player/playerPBR.gmdl";

	// Stage と同じキー。PBR シェーダを、両方で使い回す
	constexpr const char* SHADER_KEY = "Pbr";
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
		shader->Create("shader/pbrVS.hlsl", "shader/pbrPS.hlsl");
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
	m_mesh = std::move(mesh);
}

void Player::Update(uint64_t deltaTimeMicrosec)
{
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

	// 骨を動かすコードがまだないので、頂点に保存されたままの T ポーズで描かれる
	m_renderer->DrawPbr();
}

void Player::Dispose()
{
}

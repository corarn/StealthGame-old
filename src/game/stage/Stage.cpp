#include "Stage.h"
#include <iostream>
#include "../../system/CShader.h"
#include "../../system/meshmanager.h"
#include "../../system/renderer.h"

namespace
{
	constexpr const char* MODEL_PATH = "assets/model/depot/depotPBR.gmdl";
	constexpr const char* SHADER_KEY = "VertexLighting";
}

Stage::Stage(SceneBase* scene)
	: GameObject(scene)
{
}

void Stage::Init()
{
	if (!ShaderManager::Contains(SHADER_KEY))
	{
		std::unique_ptr<CShader> shader = std::make_unique<CShader>();
		shader->Create("shader/vertexLightingVS.hlsl", "shader/vertexLightingPS.hlsl");
		ShaderManager::Register<CShader>(SHADER_KEY, std::move(shader));
	}

	std::unique_ptr<CGmdlMesh> mesh = std::make_unique<CGmdlMesh>();
	if (!mesh->Load(MODEL_PATH))
	{
		std::cout << "Stage: " << MODEL_PATH << " を読めなかった: " << mesh->GetLastError() << std::endl;
		return;
	}

	m_renderer = std::make_unique<CGmdlMeshRenderer>();
	m_renderer->Init(*mesh);
	m_mesh = std::move(mesh);
}

void Stage::Update(uint64_t deltaTimeMicrosec)
{
}

void Stage::Draw(uint64_t deltaTimeMicrosec)
{
	if (!m_renderer)
	{
		return;
	}

	ShaderManager::Get<CShader>(SHADER_KEY)->SetGPU();

	Matrix4x4 world = m_srt.GetMatrix();
	Renderer::SetWorldMatrix(&world);

	m_renderer->Draw();
}

void Stage::Dispose()
{
}

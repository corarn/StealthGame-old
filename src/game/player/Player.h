#pragma once
#include <cstdint>
#include <memory>
#include "../../gameobject/GameObject.h"
#include "../../system/CGmdlMesh.h"
#include "../../system/CGmdlMeshRenderer.h"

class Player : public GameObject
{
private:
	std::unique_ptr<CGmdlMesh>         m_mesh     = nullptr;
	std::unique_ptr<CGmdlMeshRenderer> m_renderer = nullptr;

public:
	explicit Player(SceneBase* scene);

	void Update(uint64_t deltaTimeMicrosec) override;
	void Draw(uint64_t deltaTimeMicrosec) override;
	void Init() override;
	void Dispose() override;
};

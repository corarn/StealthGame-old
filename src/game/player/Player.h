#pragma once
#include <cstdint>
#include <memory>
#include "../../gameobject/GameObject.h"
#include "../../system/CGmdlMesh.h"
#include "../../system/CGmdlMeshRenderer.h"
#include "../../system/CGmdlSkeleton.h"
#include "../../system/CGmdlBonePalette.h"

class Player : public GameObject
{
private:
	std::unique_ptr<CGmdlMesh>         m_mesh     = nullptr;
	std::unique_ptr<CGmdlMeshRenderer> m_renderer = nullptr;

	// 骨。アニメの姿勢を入れると、66 本の骨の行列を作る
	std::unique_ptr<CGmdlSkeleton>    m_skeleton    = nullptr;

	// 骨の行列を GPU（b5）に送る係
	std::unique_ptr<CGmdlBonePalette> m_bonePalette = nullptr;

public:
	explicit Player(SceneBase* scene);

	void Update(uint64_t deltaTimeMicrosec) override;
	void Draw(uint64_t deltaTimeMicrosec) override;
	void Init() override;
	void Dispose() override;
};

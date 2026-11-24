#pragma once
#include <cstdint>
#include <memory>
#include "../../gameobject/GameObject.h"
#include "../../system/CGmdlMesh.h"
#include "../../system/CGmdlMeshRenderer.h"
#include "../../system/CGmdlSkeleton.h"
#include "../../system/CGmdlBonePalette.h"
#include "../animation/AnimationPlayer.h"
#include "../character/CharacterController.h"

class Player : public GameObject
{
private:
	std::unique_ptr<CGmdlMesh>         m_mesh     = nullptr;
	std::unique_ptr<CGmdlMeshRenderer> m_renderer = nullptr;

	// 骨。アニメの姿勢を入れると、66 本の骨の行列を作る
	std::unique_ptr<CGmdlSkeleton>    m_skeleton    = nullptr;

	// 骨の行列を GPU（b5）に送る係
	std::unique_ptr<CGmdlBonePalette> m_bonePalette = nullptr;

	// 今流しているアニメ。Idle・Walk・Run を切り替える
	std::unique_ptr<AnimationPlayer> m_animationPlayer = nullptr;

	// 頭。どっちへ進むかを教えてもらう。中身は PlayerController だが、体は親の型でしか知らない
	std::unique_ptr<CharacterController> m_controller = nullptr;

public:
	explicit Player(SceneBase* scene);

	void Update(uint64_t deltaTimeMicrosec) override;
	void Draw(uint64_t deltaTimeMicrosec) override;
	void Init() override;
	void Dispose() override;
};

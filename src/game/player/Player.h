#pragma once
#include <cstdint>
#include <memory>
#include "../../gameobject/GameObject.h"
#include "../../system/CGmdlMesh.h"
#include "../../system/CGmdlMeshRenderer.h"
#include "../../system/CGmdlSkeleton.h"
#include "../../system/CGmdlBonePalette.h"
#include "../animation/AnimationPlayer.h"

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

	// WASD の押し具合。x は右が +1・左が -1、y は前が +1・後ろが -1。何も押していなければ (0, 0)
	Vector2 ReadMoveInput() const;

	// 入力を、今のカメラから見た向きに直す（W ＝ 画面の奥）。地面に沿った向きで、長さは 0 か 1
	Vector3 ConvertToCameraDirection(const Vector2& moveInput) const;

public:
	explicit Player(SceneBase* scene);

	void Update(uint64_t deltaTimeMicrosec) override;
	void Draw(uint64_t deltaTimeMicrosec) override;
	void Init() override;
	void Dispose() override;
};

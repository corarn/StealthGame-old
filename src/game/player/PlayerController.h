#pragma once
#include "../character/CharacterController.h"

class SceneBase;

// プレイヤーの頭。WASD を読んで、カメラから見た向きに直して体に教える
class PlayerController : public CharacterController
{
private:
	// カメラを聞くためのシーン。シーンは体（Character）より長く生きるので、ポインタで持っても消えた物を指さない
	SceneBase* m_scene = nullptr;

	// WASD の押し具合。x は右が +1・左が -1、y は前が +1・後ろが -1。何も押していなければ (0, 0)
	Vector2 ReadMoveInput() const;

	// 入力を、今のカメラから見た向きに直す（W ＝ 画面の奥）。地面に沿った向きで、長さは 0 か 1
	Vector3 ConvertToCameraDirection(const Vector2& moveInput) const;

public:
	explicit PlayerController(SceneBase& scene);

	Vector3 DecideMoveDirection() override;
};

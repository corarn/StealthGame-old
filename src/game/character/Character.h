#pragma once
#include <cstdint>
#include <memory>
#include "../../gameobject/GameObject.h"
#include "../../system/CGmdlMesh.h"
#include "../../system/CGmdlMeshRenderer.h"
#include "../../system/CGmdlSkeleton.h"
#include "../../system/CGmdlBonePalette.h"
#include "../animation/AnimationPlayer.h"
#include "CharacterController.h"

// キャラクターの「体」。モデル・アニメ・骨を持ち、頭に聞いた向きへ動いて、その方を向く。
// どっちへ進むかは自分で決めない。プレイヤーも敵もこのクラス1つで、付ける頭（CharacterController の子）だけを変える
class Character : public GameObject
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

	// 頭。どっちへ進むかを教えてもらう。体は親の型でしか知らないので、中身がプレイヤー用か敵用かを知らない
	std::unique_ptr<CharacterController> m_controller = nullptr;

	// 向きたい角度（Y 軸まわり）。止まってもここまで回り切るので、最後に進もうとした方を覚えておく
	float m_targetYawRad = 0.0f;

public:
	explicit Character(SceneBase* scene);

	void Update(uint64_t deltaTimeMicrosec) override;
	void Draw(uint64_t deltaTimeMicrosec) override;
	void Init() override;
	void Dispose() override;

	// 頭を付ける。AddGameObject で作ったあと、シーンが呼ぶ。付けないと、ずっと立ったまま進まない
	void SetController(std::unique_ptr<CharacterController> controller);
};

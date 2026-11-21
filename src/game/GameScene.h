#pragma once
#include <cstdint>
#include "../system/IScene.h"
#include "../system/SceneClassFactory.h"

// ゲーム本編のシーン
class GameScene : public IScene
{
public:
	void update(uint64_t deltaTimeMicrosec) override;  // 毎フレームの更新
	void draw(uint64_t deltaTimeMicrosec) override;	   // 毎フレームの描画
	void init() override;						       // シーンが始まるとき
	void dispose() override;					       // シーンが終わるとき
};

REGISTER_CLASS(GameScene)
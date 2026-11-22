#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include "../../system/IScene.h"
#include "../../system/SceneClassFactory.h"
#include "../../system/C3DShape.h"
#include "../../gameobject/gameobject.h"

class GameScene : public SceneBase
{
private:
	static constexpr std::size_t AXIS_COUNT{ 3 };  // X・Y・Z

	static constexpr std::array<Vector3, AXIS_COUNT> AXIS_DIRECTIONS
	{
		Vector3{ 1.0f, 0.0f, 0.0f },  // X
		Vector3{ 0.0f, 1.0f, 0.0f },  // Y
		Vector3{ 0.0f, 0.0f, 1.0f }   // Z
	};
	static constexpr std::array<Color, AXIS_COUNT> AXIS_COLORS
	{
		Color{ 1.0f, 0.0f, 0.0f },  // 赤
		Color{ 0.0f, 1.0f, 0.0f },  // 緑
		Color{ 0.0f, 0.0f, 1.0f }   // 青
	};

	/// 軸の線1本の持ち主。Segment は空で作れないので init() で作る
	using AxisLine = std::unique_ptr<Segment>;

	// 原点に出す X・Y・Z の軸の線。マップが見えないとき、読み込みとカメラのどっちが悪いか見分ける用
	std::array<AxisLine, AXIS_COUNT> m_axisLines{};

public:
	void update(uint64_t deltaTimeMicrosec) override;
	void draw(uint64_t deltaTimeMicrosec) override;
	void init() override;
	void dispose() override;
};

REGISTER_CLASS(GameScene)
#pragma once
#include "../../system/commontypes.h"

// キャラクターの「頭」。体（Character）に、どっちへ進むかを教える。
// 体は頭の中身（キー入力か AI か）を知らない。プレイヤー用と敵用で、同じ呼び方で違う決め方をする。
// 現場の Unreal の Controller と同じ考え方で、体と頭を分けた
class CharacterController
{
public:
	// 親の型の unique_ptr で持って消すので、子のデストラクタも呼ばれるように virtual にする
	virtual ~CharacterController() = default;

	// 進む向きを返す。地面に沿った向きで、長さは 0（進まない）か 1。
	// const にしないのは、敵の頭が「巡回で次にどこへ行くか」を覚えて書き換えるから
	virtual Vector3 DecideMoveDirection() = 0;
};

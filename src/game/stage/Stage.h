#pragma once
#include <cstdint>
#include "../../gameobject/GameObject.h"

class Stage : public GameObject
{
public:
	explicit Stage(SceneBase* scene);

	void Update(uint64_t deltaTimeMicrosec) override;
	void Draw(uint64_t deltaTimeMicrosec) override;
	void Init() override;
	void Dispose() override;
};

#include "IScene.h"
#include "../gameobject/GameObject.h"

void SceneBase::UpdateGameObjects(uint64_t deltaTimeMicrosec)
{
	for (uint32_t layer = 0; layer < LAYERMAX; ++layer)
	{
		for (std::unique_ptr<GameObject>& object : m_gameobjects[layer])
		{
			if (object && !object->isDestroyed())
			{
				object->Update(deltaTimeMicrosec);
			}
		}
	}

	for (uint32_t layer = 0; layer < LAYERMAX; ++layer)
	{
		std::erase_if(m_gameobjects[layer], [](const std::unique_ptr<GameObject>& object)
			{
				return object == nullptr || object->isDestroyed();
			});
	}

	for (uint32_t layer = 0; layer < LAYERMAX; ++layer)
	{
		for (std::unique_ptr<GameObject>& object : m_pendingAddObjects[layer])
		{
			m_gameobjects[layer].push_back(std::move(object));
		}
		m_pendingAddObjects[layer].clear();
	}
}

void SceneBase::DrawGameObjects(uint64_t deltaTimeMicrosec)
{
	for (uint32_t layer = 0; layer < LAYERMAX; ++layer)
	{
		for (std::unique_ptr<GameObject>& object : m_gameobjects[layer])
		{
			if (object && !object->isDestroyed())
			{
				object->Draw(deltaTimeMicrosec);
			}
		}
	}
}

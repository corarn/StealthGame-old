#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <array>
#include <cassert>
#include <typeinfo>
#include <utility>
#include "camera.h"

// 前方宣言
class GameObject;

// シーンインタフェース
class IScene {
public:
	IScene() = default;
	virtual ~IScene() = default;
	virtual void Update(uint64_t delta) = 0;
	virtual void Draw(uint64_t delta) = 0;
	virtual void Init() = 0;
	virtual void Dispose() = 0;
};

class SceneBase : public IScene {
public:
	static constexpr uint32_t LAYERMAX = 4;

protected:
	using ObjectList = std::vector<std::unique_ptr<GameObject>>;
	using LayerArray = std::array<ObjectList, LAYERMAX>;

	LayerArray m_gameobjects;			// 描画・更新対象
	LayerArray m_pendingAddObjects;		// 次フレームで追加するオブジェクト

	std::unique_ptr<Camera> m_camera;	// カメラ

	// 物のリストを回す。シーンの Update・Draw から呼ぶ add 20261122
	void UpdateGameObjects(uint64_t deltaTimeMicrosec);
	void DrawGameObjects(uint64_t deltaTimeMicrosec);

public:
	Camera* getcamera() { return m_camera.get(); }

	template <typename T>
	T* AddGameObject(int layer)
	{
		assert(layer >= 0 && layer < static_cast<int>(LAYERMAX));

		auto obj = std::make_unique<T>(this);
		obj->Init();

		T* rawPtr = obj.get();
		m_pendingAddObjects[layer].push_back(std::move(obj));

		return rawPtr;
	}

	template <typename T>
	T* GetGameObject()
	{
		for (auto& objectList : m_gameobjects)
		{
			for (auto& object : objectList)		// object は unique_ptr<GameObject>&
			{
				if (typeid(*object) == typeid(T))	// RTTI で厳密に型一致を判定
				{
					return static_cast<T*>(object.get());
				}
			}
		}
		return nullptr;
	}

	template <typename T>
	std::vector<T*> GetGameObjects()
	{
		std::vector<T*> objects;

		for (auto& objectList : m_gameobjects)
		{
			for (auto& object : objectList)
			{
				if (typeid(*object) == typeid(T))
				{
					objects.push_back(static_cast<T*>(object.get()));
				}
			}
		}
		return objects;
	}
};
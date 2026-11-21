#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include "CMesh.h"
#include "CMeshRenderer.h"
#include "CShader.h"
#include "noncopyable.h"

template <typename ResourceType>
class ResourceManager : NonCopyable {
private:
    // C++17 inline static: プログラム全体で1つだけ存在するコンテナ
    static inline std::unordered_map<std::string, std::unique_ptr<ResourceType>> m_container{};

public:
    template<class T = ResourceType>
    static bool Register(const std::string& key, std::unique_ptr<T> data) {
        if (m_container.contains(key)) return false;
        m_container.emplace(key, std::move(data));
        return true;
    }

    template<class T = ResourceType>
    static T* Get(const std::string& key) {
        auto it = m_container.find(key);
        if (it != m_container.end()) {
            return static_cast<T*>(it->second.get());
        }
        return nullptr;
    }

    static bool Contains(const std::string& key) {
        return m_container.contains(key);
    }

    // デバイス終了前に安全に解放するためのクリア関数
    static void Clear() {
        m_container.clear();
    }
};

// Shader用のマネージャ
using ShaderManager = ResourceManager<CShader>;

// Mesh / MeshRenderer 用のマネージャ
class MeshManager : NonCopyable {
public:
    // --- Mesh ---
    template<class T = CMesh>
    static bool RegisterMesh(const std::string& key, std::unique_ptr<T> meshdata) {
        return ResourceManager<CMesh>::Register<T>(key, std::move(meshdata));
    }

    template<class T = CMesh>
    static T* GetMesh(const std::string& key) {
        return ResourceManager<CMesh>::Get<T>(key);
    }

    static bool ContainsMesh(const std::string& key) {
        return ResourceManager<CMesh>::Contains(key);
    }

    // --- MeshRenderer ---
    template<class T = CMeshRenderer>
    static bool RegisterMeshRenderer(const std::string& key, std::unique_ptr<T> data) {
        return ResourceManager<CMeshRenderer>::Register<T>(key, std::move(data));
    }

    template<class T = CMeshRenderer>
    static T* GetRenderer(const std::string& key) {
        return ResourceManager<CMeshRenderer>::Get<T>(key);
    }

    static bool ContainsRenderer(const std::string& key) {
        return ResourceManager<CMeshRenderer>::Contains(key);
    }

    // まとめてリソース破棄
    static void ClearAll() {
        ResourceManager<CMesh>::Clear();
        ResourceManager<CMeshRenderer>::Clear();
    }
};
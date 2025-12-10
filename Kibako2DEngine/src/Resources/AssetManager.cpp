// Manages loading and caching of GPU textures
#include "KibakoEngine/Resources/AssetManager.h"

#include "KibakoEngine/Core/Debug.h"
#include "KibakoEngine/Core/Log.h"

namespace KibakoEngine {

    namespace
    {
        constexpr const char* kLogChannel = "Assets";
    }

    void AssetManager::Init(ID3D11Device* device)
    {
        KBK_ASSERT(device != nullptr, "AssetManager::Init called with null device");
        m_device = device;

    }

    void AssetManager::Shutdown()
    {
        Clear();
        m_device = nullptr;
        KbkLog(kLogChannel, "AssetManager shutdown");
    }

    void AssetManager::Clear()
    {
        m_textures.clear();
    }

    Texture2D* AssetManager::LoadTexture(const std::string& id,
        const std::string& path,
        bool sRGB)
    {
        if (!m_device) {
            KbkError(kLogChannel,
                "Cannot load texture '%s' (id='%s'): device is null",
                path.c_str(), id.c_str());
            return nullptr;
        }

        auto it = m_textures.find(id);
        if (it != m_textures.end()) {
            KbkTrace(kLogChannel,
                "Reusing already loaded texture '%s' (id='%s')",
                path.c_str(), id.c_str());
            return it->second.get();
        }

        auto texture = std::make_unique<Texture2D>();
        if (!texture->LoadFromFile(m_device, path, sRGB)) {
            KbkError(kLogChannel,
                "Failed to load texture from '%s' (id='%s')",
                path.c_str(), id.c_str());
            return nullptr;
        }

        Texture2D* result = texture.get();
        m_textures.emplace(id, std::move(texture));

        KbkLog(kLogChannel,
            "Loaded texture '%s' as id='%s' (%dx%d)",
            path.c_str(), id.c_str(), result->Width(), result->Height());

        return result;
    }

    Texture2D* AssetManager::GetTexture(const std::string& id)
    {
        auto it = m_textures.find(id);
        if (it == m_textures.end())
            return nullptr;
        return it->second.get();
    }

    const Texture2D* AssetManager::GetTexture(const std::string& id) const
    {
        auto it = m_textures.find(id);
        if (it == m_textures.end())
            return nullptr;
        return it->second.get();
    }

} // namespace KibakoEngine

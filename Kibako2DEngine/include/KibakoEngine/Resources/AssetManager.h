// Simple asset manager that caches textures
#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "KibakoEngine/Renderer/Texture2D.h"

struct ID3D11Device;

namespace KibakoEngine {

class AssetManager
{
public:
    AssetManager() = default;

    void Init(ID3D11Device* device);
    void Shutdown();

    // Load a texture from disk or return an existing one by id
    [[nodiscard]] Texture2D* LoadTexture(const std::string& id,
                                         const std::string& path,
                                         bool sRGB = true);

    // Look up a texture by id, returns nullptr when not found
    [[nodiscard]] Texture2D* GetTexture(const std::string& id);
    [[nodiscard]] const Texture2D* GetTexture(const std::string& id) const;

    // Release all loaded textures
    void Clear();

private:
    ID3D11Device* m_device = nullptr;
    std::unordered_map<std::string, std::unique_ptr<Texture2D>> m_textures;
};

} // namespace KibakoEngine


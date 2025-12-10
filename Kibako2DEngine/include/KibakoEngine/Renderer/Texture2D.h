// Lightweight wrapper around a Direct3D 11 2D texture
#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <string>
#include <cstdint>

namespace KibakoEngine {

    class Texture2D {
    public:
        bool LoadFromFile(ID3D11Device* device, const std::string& path, bool srgb = false);
        bool CreateFromRGBA8(ID3D11Device* device,
                             int width,
                             int height,
                             const std::uint8_t* pixels);
        bool CreateSolidColor(ID3D11Device* device,
                              std::uint8_t r,
                              std::uint8_t g,
                              std::uint8_t b,
                              std::uint8_t a = 255);
        void Reset();

        [[nodiscard]] int Width() const { return m_width; }
        [[nodiscard]] int Height() const { return m_height; }
        [[nodiscard]] ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }
        [[nodiscard]] bool IsValid() const { return m_srv != nullptr; }

    private:
        Microsoft::WRL::ComPtr<ID3D11Texture2D>        m_texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srv;
        int m_width = 0;
        int m_height = 0;
    };

} // namespace KibakoEngine


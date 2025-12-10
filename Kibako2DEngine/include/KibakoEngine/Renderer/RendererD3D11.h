// Direct3D 11 renderer that owns the swap chain, camera, and sprite batch
#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <cstdint>

#include "KibakoEngine/Renderer/Camera2D.h"
#include "KibakoEngine/Renderer/SpriteBatch2D.h"

struct HWND__;
using HWND = HWND__*;

namespace KibakoEngine {

    class RendererD3D11 {
    public:
        bool Init(HWND hwnd, uint32_t width, uint32_t height);
        void Shutdown();

        void BeginFrame(const float clearColor[4]);
        void EndFrame(bool waitForVSync);
        void OnResize(uint32_t width, uint32_t height);

        const D3D11_VIEWPORT& GetViewport() const { return m_viewport; }

        [[nodiscard]] ID3D11Device* GetDevice() const { return m_device.Get(); }
        [[nodiscard]] ID3D11DeviceContext* GetImmediateContext() const { return m_context.Get(); }
        [[nodiscard]] Camera2D& Camera() { return m_camera; }
        [[nodiscard]] SpriteBatch2D& Batch() { return m_batch; }

        // Backbuffer resolution (native 1:1)
        [[nodiscard]] uint32_t BackBufferWidth() const { return m_backBufferWidth; }
        [[nodiscard]] uint32_t BackBufferHeight() const { return m_backBufferHeight; }

        // Logical resolution is kept for compatibility and mirrors the backbuffer.
        [[nodiscard]] uint32_t LogicalWidth() const { return m_backBufferWidth; }
        [[nodiscard]] uint32_t LogicalHeight() const { return m_backBufferHeight; }

    private:
        bool CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height);
        bool CreateRenderTargets(uint32_t width, uint32_t height);
        void UpdateViewport(uint32_t backBufferWidth, uint32_t backBufferHeight);

        Microsoft::WRL::ComPtr<ID3D11Device>           m_device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext>    m_context;
        Microsoft::WRL::ComPtr<IDXGISwapChain>         m_swapChain;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
        D3D_FEATURE_LEVEL m_featureLevel = D3D_FEATURE_LEVEL_11_0;

        Camera2D      m_camera;
        SpriteBatch2D m_batch;

        uint32_t m_backBufferWidth = 0;
        uint32_t m_backBufferHeight = 0;

        D3D11_VIEWPORT m_viewport{};
    };

} // namespace KibakoEngine
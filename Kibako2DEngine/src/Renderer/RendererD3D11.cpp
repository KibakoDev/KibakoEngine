// Sets up and drives the Direct3D 11 renderer
#ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#    define NOMINMAX
#endif

#include "KibakoEngine/Renderer/RendererD3D11.h"

#include "KibakoEngine/Core/Debug.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/Core/Profiler.h"

#include <windows.h>
#include <dxgi.h>

#include <iterator>
#include <algorithm>

#ifdef _MSC_VER
#    pragma comment(lib, "d3d11.lib")
#    pragma comment(lib, "dxgi.lib")
#endif

namespace KibakoEngine {

    namespace {
        constexpr const char* kLogChannel = "Renderer";
    }

    bool RendererD3D11::Init(HWND hwnd, std::uint32_t width, std::uint32_t height)
    {
        KBK_PROFILE_SCOPE("RendererInit");

        if (!CreateSwapChain(hwnd, width, height)) {
            KbkError(kLogChannel, "Failed to create swap chain");
            return false;
        }
        if (!CreateRenderTargets(width, height)) {
            KbkError(kLogChannel, "Failed to create render targets");
            return false;
        }

        // Backbuffer is now our canonical resolution (1:1 rendering).
        UpdateViewport(width, height);

        if (!m_batch.Init(m_device.Get(), m_context.Get())) {
            KbkError(kLogChannel, "SpriteBatch2D initialization failed");
            return false;
        }

        m_camera.SetPosition(0.0f, 0.0f);
        m_camera.SetRotation(0.0f);
        // Camera viewport is updated inside UpdateViewport().

        return true;
    }

    void RendererD3D11::Shutdown()
    {
        KBK_PROFILE_SCOPE("RendererShutdown");

        m_batch.Shutdown();
        if (m_context)
            m_context->ClearState();
        m_rtv.Reset();
        m_swapChain.Reset();
        m_context.Reset();
        m_device.Reset();
        m_backBufferWidth = 0;
        m_backBufferHeight = 0;
        m_viewport = {};
    }

    void RendererD3D11::BeginFrame(const float clearColor[4])
    {
        KBK_PROFILE_SCOPE("RendererBeginFrame");

        const float color[4] = {
            clearColor ? clearColor[0] : 0.0f,
            clearColor ? clearColor[1] : 0.0f,
            clearColor ? clearColor[2] : 0.0f,
            clearColor ? clearColor[3] : 1.0f,
        };

        ID3D11RenderTargetView* rtv = m_rtv.Get();
        m_context->OMSetRenderTargets(1, &rtv, nullptr);

        // Fullscreen viewport in backbuffer space (1:1 rendering).
        m_context->RSSetViewports(1, &m_viewport);

        m_context->ClearRenderTargetView(m_rtv.Get(), color);
    }

    void RendererD3D11::EndFrame(bool waitForVSync)
    {
        KBK_PROFILE_SCOPE("RendererEndFrame");

        if (!m_swapChain)
            return;

        const HRESULT hr = m_swapChain->Present(waitForVSync ? 1 : 0, 0);
        if (FAILED(hr)) {
            KbkError(kLogChannel,
                "Swap chain Present failed: 0x%08X",
                static_cast<unsigned>(hr));
        }
    }

    void RendererD3D11::OnResize(std::uint32_t width, std::uint32_t height)
    {
        KBK_PROFILE_SCOPE("RendererResize");

        if (!m_swapChain || width == 0 || height == 0)
            return;
        if (width == m_backBufferWidth && height == m_backBufferHeight)
            return;

        m_context->OMSetRenderTargets(0, nullptr, nullptr);
        m_rtv.Reset();

        const HRESULT hr = m_swapChain->ResizeBuffers(
            0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr)) {
            KbkError(kLogChannel,
                "ResizeBuffers failed: 0x%08X",
                static_cast<unsigned>(hr));
            return;
        }

        if (!CreateRenderTargets(width, height)) {
            KbkError(kLogChannel,
                "Failed to recreate render targets after resize");
            return;
        }

        // Backbuffer size is our new canonical resolution (1:1).
        UpdateViewport(width, height);
    }

    bool RendererD3D11::CreateSwapChain(HWND hwnd,
        std::uint32_t width,
        std::uint32_t height)
    {
        KBK_PROFILE_SCOPE("CreateSwapChain");

        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if KBK_DEBUG_BUILD
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferCount = 2;
        desc.BufferDesc.Width = width;
        desc.BufferDesc.Height = height;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = hwnd;
        desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        D3D_FEATURE_LEVEL requestedLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags,
            requestedLevels,
            static_cast<UINT>(std::size(requestedLevels)),
            D3D11_SDK_VERSION,
            &desc,
            m_swapChain.GetAddressOf(),
            m_device.GetAddressOf(),
            &m_featureLevel,
            m_context.GetAddressOf());
#if KBK_DEBUG_BUILD
        if (FAILED(hr)) {
            KbkWarn(kLogChannel,
                "Retrying device creation without debug layer");
            flags &= ~D3D11_CREATE_DEVICE_DEBUG;
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr,
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                flags,
                requestedLevels,
                static_cast<UINT>(std::size(requestedLevels)),
                D3D11_SDK_VERSION,
                &desc,
                m_swapChain.GetAddressOf(),
                m_device.GetAddressOf(),
                &m_featureLevel,
                m_context.GetAddressOf());
        }
#endif

        if (FAILED(hr))
            return false;

        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
        if (SUCCEEDED(m_device.As(&dxgiDevice)) && dxgiDevice) {
            Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
            if (SUCCEEDED(dxgiDevice->GetAdapter(adapter.GetAddressOf())) && adapter) {
                Microsoft::WRL::ComPtr<IDXGIFactory> factory;
                if (SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf()))) && factory) {
                    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
                }
            }
        }

        KbkLog(kLogChannel,
            "D3D11 feature level: 0x%04X",
            static_cast<unsigned>(m_featureLevel));
        return true;
    }

    bool RendererD3D11::CreateRenderTargets(std::uint32_t width, std::uint32_t height)
    {
        KBK_PROFILE_SCOPE("CreateRenderTargets");

        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        HRESULT hr = m_swapChain->GetBuffer(
            0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
        if (FAILED(hr)) {
            KbkError(kLogChannel,
                "GetBuffer failed: 0x%08X",
                static_cast<unsigned>(hr));
            return false;
        }

        hr = m_device->CreateRenderTargetView(
            backBuffer.Get(), nullptr, m_rtv.GetAddressOf());
        if (FAILED(hr)) {
            KbkError(kLogChannel,
                "CreateRenderTargetView failed: 0x%08X",
                static_cast<unsigned>(hr));
            return false;
        }

        m_backBufferWidth = width;
        m_backBufferHeight = height;

        return true;
    }

    void RendererD3D11::UpdateViewport(std::uint32_t backBufferWidth,
        std::uint32_t backBufferHeight)
    {
        if (backBufferWidth == 0 || backBufferHeight == 0) {
            m_viewport = {};
            return;
        }

        const float w = static_cast<float>(backBufferWidth);
        const float h = static_cast<float>(backBufferHeight);

        m_viewport.TopLeftX = 0.0f;
        m_viewport.TopLeftY = 0.0f;
        m_viewport.Width = w;
        m_viewport.Height = h;
        m_viewport.MinDepth = 0.0f;
        m_viewport.MaxDepth = 1.0f;

        // Camera uses the same dimensions: 1 unit = 1 pixel in screen space.
        m_camera.SetViewport(w, h);
    }

} // namespace KibakoEngine
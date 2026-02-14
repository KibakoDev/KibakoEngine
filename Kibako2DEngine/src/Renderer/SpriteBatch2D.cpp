// Batches sprites and custom geometry for efficient Direct3D 11 rendering
#include "KibakoEngine/Renderer/SpriteBatch2D.h"

#include "KibakoEngine/Core/Debug.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/Core/Profiler.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace DirectX;

namespace KibakoEngine {

    namespace {
        constexpr const char* kLogChannel = "SpriteBatch";
    }

    const Texture2D* SpriteBatch2D::DefaultWhiteTexture() const
    {
        return m_defaultWhite.IsValid() ? &m_defaultWhite : nullptr;
    }

    bool SpriteBatch2D::Init(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        KBK_PROFILE_SCOPE("SpriteBatchInit");

        KBK_ASSERT(device != nullptr, "SpriteBatch2D::Init requires device");
        KBK_ASSERT(context != nullptr, "SpriteBatch2D::Init requires context");
        m_device = device;
        m_context = context;

        if (!CreateShaders(device)) {
            KbkError(kLogChannel, "Failed to create shaders");
            return false;
        }
        if (!CreateStates(device)) {
            KbkError(kLogChannel, "Failed to create states");
            return false;
        }

        if (!EnsureVertexCapacity(256 * 4) || !EnsureIndexCapacity(256 * 6))
            return false;

        if (!m_defaultWhite.CreateSolidColor(device, 255, 255, 255, 255)) {
            KbkWarn(kLogChannel, "Failed to create default white texture for SpriteBatch2D");
        }

        return true;
    }

    void SpriteBatch2D::Shutdown()
    {
        KBK_PROFILE_SCOPE("SpriteBatchShutdown");

        ClearFrameData();

        m_defaultWhite.Reset();

        m_vertexBuffer.Reset();
        m_indexBuffer.Reset();
        m_cbVS.Reset();
        m_vs.Reset();
        m_ps.Reset();
        m_inputLayout.Reset();
        m_samplerPoint.Reset();
        m_blendAlpha.Reset();
        m_depthDisabled.Reset();
        m_rasterCullNone.Reset();
        m_rasterCullNoneScissor.Reset();

        m_device = nullptr;
        m_context = nullptr;
        m_vertexCapacity = 0;
        m_indexCapacity = 0;
    }

    void SpriteBatch2D::ClearFrameData()
    {
        m_indexScratch.clear();
        m_vertexScratch.clear();
        m_commands.clear();
        m_geometryCommands.clear();
        m_unifiedCommands.clear();
        m_drawRanges.clear();
    }

    void SpriteBatch2D::Begin(const XMFLOAT4X4& viewProjT)
    {
        KBK_PROFILE_SCOPE("SpriteBatchBegin");

        m_stats = {};

        KBK_ASSERT(!m_isDrawing, "SpriteBatch2D::Begin without End");
        m_isDrawing = true;
        m_viewProjT = viewProjT;

        m_commands.clear();
        m_geometryCommands.clear();
    }

    void SpriteBatch2D::End()
    {
        KBK_PROFILE_SCOPE("SpriteBatchEnd");

        KBK_ASSERT(m_isDrawing, "SpriteBatch2D::End without Begin");
        m_isDrawing = false;

        // Drop any sprite commands missing a valid texture
        m_commands.erase(
            std::remove_if(
                m_commands.begin(), m_commands.end(),
                [](const DrawCommand& cmd) {
                    return cmd.texture == nullptr || cmd.texture->GetSRV() == nullptr;
                }),
            m_commands.end()
        );

        // Drop any geometry commands that cannot be rendered
        m_geometryCommands.erase(
            std::remove_if(
                m_geometryCommands.begin(), m_geometryCommands.end(),
                [](const GeometryCommand& cmd) {
                    return cmd.vertices.empty() || cmd.indices.empty() ||
                        cmd.texture == nullptr || cmd.texture->GetSRV() == nullptr;
                }),
            m_geometryCommands.end()
        );

        if (m_commands.empty() && m_geometryCommands.empty())
            return;

        // Merge sprite and geometry commands into a unified list
        using Unified = UnifiedCommand;
        m_unifiedCommands.clear();
        m_unifiedCommands.reserve(m_commands.size() + m_geometryCommands.size());

        for (size_t i = 0; i < m_commands.size(); ++i) {
            const auto& c = m_commands[i];
            m_unifiedCommands.push_back({ c.texture, c.layer, true, i });
        }
        for (size_t i = 0; i < m_geometryCommands.size(); ++i) {
            const auto& g = m_geometryCommands[i];
            m_unifiedCommands.push_back({ g.texture, g.layer, false, i });
        }

        // Sort by layer then texture to maximize batching
        std::stable_sort(
            m_unifiedCommands.begin(), m_unifiedCommands.end(),
            [this](const Unified& a, const Unified& b) {
                if (a.layer != b.layer)
                    return a.layer < b.layer;
                const auto srvA = a.texture->GetSRV();
                const auto srvB = b.texture->GetSRV();
                if (srvA != srvB)
                    return srvA < srvB;

                if (a.isSprite != b.isSprite)
                    return a.isSprite;

                if (!a.isSprite) {
                    const auto& ga = m_geometryCommands[a.index];
                    const auto& gb = m_geometryCommands[b.index];
                    if (ga.hasClipRect != gb.hasClipRect)
                        return !ga.hasClipRect;

                    if (ga.hasClipRect) {
                        if (ga.clipRect.x != gb.clipRect.x) return ga.clipRect.x < gb.clipRect.x;
                        if (ga.clipRect.y != gb.clipRect.y) return ga.clipRect.y < gb.clipRect.y;
                        if (ga.clipRect.w != gb.clipRect.w) return ga.clipRect.w < gb.clipRect.w;
                        if (ga.clipRect.h != gb.clipRect.h) return ga.clipRect.h < gb.clipRect.h;
                    }
                }

                return a.index < b.index;
            });

        // Precompute the total vertex and index counts
        size_t totalVertices = 0;
        size_t totalIndices = 0;
        for (const Unified& u : m_unifiedCommands) {
            if (u.isSprite) {
                totalVertices += 4;
                totalIndices += 6;
            }
            else {
                const auto& g = m_geometryCommands[u.index];
                totalVertices += g.vertices.size();
                totalIndices += g.indices.size();
            }
        }

        if (totalVertices == 0 || totalIndices == 0)
            return;

        if (!EnsureVertexCapacity(totalVertices) || !EnsureIndexCapacity(totalIndices))
            return;

        UpdateVSConstants();

        m_vertexScratch.clear();
        m_vertexScratch.resize(totalVertices);

        m_indexScratch.clear();
        m_indexScratch.resize(totalIndices);

        // Build the final GPU buffers and draw ranges
        m_drawRanges.clear();
        m_drawRanges.reserve(m_unifiedCommands.size());

        size_t currentVertexBase = 0;
        size_t currentIndexBase = 0;
        bool   haveRange = false;
        DrawRange currentRange{};

        for (const Unified& u : m_unifiedCommands) {
            const std::uint32_t cmdFirstIndex = static_cast<std::uint32_t>(currentIndexBase);

            if (u.isSprite) {
                const DrawCommand& cmd = m_commands[u.index];

                const float left = cmd.dst.x;
                const float top = cmd.dst.y;
                const float right = cmd.dst.x + cmd.dst.w;
                const float bottom = cmd.dst.y + cmd.dst.h;

                XMFLOAT2 corners[4] = {
                    { left,  top    },
                    { right, top    },
                    { right, bottom },
                    { left,  bottom },
                };

                if (std::fabs(cmd.rotation) > 0.0001f) {
                    const float cx = cmd.dst.x + cmd.dst.w * 0.5f;
                    const float cy = cmd.dst.y + cmd.dst.h * 0.5f;
                    const float cs = std::cos(cmd.rotation);
                    const float sn = std::sin(cmd.rotation);
                    for (auto& p : corners) {
                        const float dx = p.x - cx;
                        const float dy = p.y - cy;
                        p.x = cx + dx * cs - dy * sn;
                        p.y = cy + dx * sn + dy * cs;
                    }
                }

                const float u0 = cmd.src.x;
                const float v0 = cmd.src.y;
                const float u1 = cmd.src.x + cmd.src.w;
                const float v1 = cmd.src.y + cmd.src.h;

                const XMFLOAT4 color = { cmd.color.r, cmd.color.g, cmd.color.b, cmd.color.a };

                Vertex* v = m_vertexScratch.data() + currentVertexBase;

                v[0] = Vertex{ { corners[0].x, corners[0].y, 0.0f }, { u0, v0 }, color };
                v[1] = Vertex{ { corners[1].x, corners[1].y, 0.0f }, { u1, v0 }, color };
                v[2] = Vertex{ { corners[2].x, corners[2].y, 0.0f }, { u1, v1 }, color };
                v[3] = Vertex{ { corners[3].x, corners[3].y, 0.0f }, { u0, v1 }, color };

                std::uint32_t* idx = m_indexScratch.data() + currentIndexBase;
                const std::uint32_t base = static_cast<std::uint32_t>(currentVertexBase);

                idx[0] = base;
                idx[1] = base + 1;
                idx[2] = base + 2;
                idx[3] = base;
                idx[4] = base + 2;
                idx[5] = base + 3;

                currentVertexBase += 4;
                currentIndexBase += 6;
            }
            else {
                const GeometryCommand& geo = m_geometryCommands[u.index];

                // Copy vertices into the shared scratch buffer without any additional transform
                std::memcpy(
                    m_vertexScratch.data() + currentVertexBase,
                    geo.vertices.data(),
                    geo.vertices.size() * sizeof(Vertex)
                );

                // Copy indices while offsetting into the combined vertex buffer
                std::uint32_t* idxOut = m_indexScratch.data() + currentIndexBase;
                const std::uint32_t base = static_cast<std::uint32_t>(currentVertexBase);
                for (size_t i = 0; i < geo.indices.size(); ++i) {
                    idxOut[i] = base + geo.indices[i];
                }

                currentVertexBase += geo.vertices.size();
                currentIndexBase += geo.indices.size();
            }

            const std::uint32_t cmdIndexCount =
                static_cast<std::uint32_t>(currentIndexBase - cmdFirstIndex);

            bool cmdUseScissor = false;
            D3D11_RECT cmdScissor{ 0, 0, 0, 0 };
            if (!u.isSprite) {
                const GeometryCommand& geo = m_geometryCommands[u.index];
                if (geo.hasClipRect) {
                    cmdUseScissor = true;
                    cmdScissor.left = static_cast<LONG>(geo.clipRect.x);
                    cmdScissor.top = static_cast<LONG>(geo.clipRect.y);
                    cmdScissor.right = static_cast<LONG>(geo.clipRect.x + geo.clipRect.w);
                    cmdScissor.bottom = static_cast<LONG>(geo.clipRect.y + geo.clipRect.h);
                }
            }

            // Merge contiguous commands that share layer, texture and scissor state
            if (!haveRange) {
                haveRange = true;
                currentRange.texture = u.texture;
                currentRange.layer = u.layer;
                currentRange.firstIndex = cmdFirstIndex;
                currentRange.indexCount = cmdIndexCount;
                currentRange.useScissor = cmdUseScissor;
                currentRange.scissorRect = cmdScissor;
            }
            else if (u.texture == currentRange.texture &&
                     u.layer == currentRange.layer &&
                     cmdUseScissor == currentRange.useScissor &&
                     (!cmdUseScissor || (
                        cmdScissor.left == currentRange.scissorRect.left &&
                        cmdScissor.top == currentRange.scissorRect.top &&
                        cmdScissor.right == currentRange.scissorRect.right &&
                        cmdScissor.bottom == currentRange.scissorRect.bottom))) {
                currentRange.indexCount += cmdIndexCount;
            }
            else {
                m_drawRanges.push_back(currentRange);
                currentRange.texture = u.texture;
                currentRange.layer = u.layer;
                currentRange.firstIndex = cmdFirstIndex;
                currentRange.indexCount = cmdIndexCount;
                currentRange.useScissor = cmdUseScissor;
                currentRange.scissorRect = cmdScissor;
            }
        }

        if (haveRange)
            m_drawRanges.push_back(currentRange);

        // Upload the combined buffers to the GPU
        D3D11_MAPPED_SUBRESOURCE mapped{};

        HRESULT hr = m_context->Map(m_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(hr)) {
            KbkError(kLogChannel, "Vertex buffer map failed: 0x%08X", static_cast<unsigned>(hr));
            return;
        }
        std::memcpy(mapped.pData, m_vertexScratch.data(), m_vertexScratch.size() * sizeof(Vertex));
        m_context->Unmap(m_vertexBuffer.Get(), 0);

        hr = m_context->Map(m_indexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(hr)) {
            KbkError(kLogChannel, "Index buffer map failed: 0x%08X", static_cast<unsigned>(hr));
            return;
        }
        std::memcpy(mapped.pData, m_indexScratch.data(), m_indexScratch.size() * sizeof(std::uint32_t));
        m_context->Unmap(m_indexBuffer.Get(), 0);

        // Set the pipeline state used for sprite rendering
        const UINT stride = sizeof(Vertex);
        const UINT offset = 0;
        ID3D11Buffer* vb = m_vertexBuffer.Get();
        ID3D11Buffer* ib = m_indexBuffer.Get();
        m_context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        m_context->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
        m_context->IASetInputLayout(m_inputLayout.Get());
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        ID3D11Buffer* cbs[] = { m_cbVS.Get() };
        m_context->VSSetConstantBuffers(0, 1, cbs);

        m_context->VSSetShader(m_vs.Get(), nullptr, 0);
        m_context->PSSetShader(m_ps.Get(), nullptr, 0);

        const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
        m_context->OMSetBlendState(m_blendAlpha.Get(), blendFactor, 0xFFFFFFFFu);
        m_context->OMSetDepthStencilState(m_depthDisabled.Get(), 0);
        m_context->RSSetState(m_rasterCullNone.Get());

        ID3D11SamplerState* sampler = m_samplerPoint.Get();
        m_context->PSSetSamplers(0, 1, &sampler);

        for (const DrawRange& range : m_drawRanges) {
            if (range.useScissor) {
                m_context->RSSetState(m_rasterCullNoneScissor.Get());
                m_context->RSSetScissorRects(1, &range.scissorRect);
            }
            else {
                m_context->RSSetState(m_rasterCullNone.Get());
            }

            ID3D11ShaderResourceView* srv =
                range.texture ? range.texture->GetSRV()
                : (m_defaultWhite.IsValid() ? m_defaultWhite.GetSRV() : nullptr);

            m_context->PSSetShaderResources(0, 1, &srv);
            m_context->DrawIndexed(range.indexCount, range.firstIndex, 0);

            ID3D11ShaderResourceView* nullSrv = nullptr;
            m_context->PSSetShaderResources(0, 1, &nullSrv);

            m_stats.drawCalls++;
        }
    }

    void SpriteBatch2D::Push(const Texture2D& texture,
        const RectF& dst,
        const RectF& src,
        const Color4& color,
        float rotation,
        int layer)
    {
#if KBK_DEBUG_BUILD
        KBK_ASSERT(m_isDrawing, "SpriteBatch2D::Push called outside Begin/End");
#endif
        if (!m_isDrawing)
            return;

        m_commands.push_back({ &texture, dst, src, color, rotation, layer });
        m_stats.spritesSubmitted++;
    }

    void SpriteBatch2D::PushGeometryRaw(const Texture2D* texture,
        const Vertex* vertices, size_t vertexCount,
        const std::uint32_t* indices, size_t indexCount,
        int layer,
        const RectF& clipRect)
    {
#if KBK_DEBUG_BUILD
        KBK_ASSERT(m_isDrawing, "SpriteBatch2D::PushGeometryRaw called outside Begin/End");
#endif
        if (!m_isDrawing)
            return;

        if (!vertices || vertexCount == 0 || !indices || indexCount == 0)
            return;

        GeometryCommand cmd;
        cmd.texture = texture ? texture : DefaultWhiteTexture();
        cmd.layer = layer;
        cmd.hasClipRect = clipRect.w > 0.0f && clipRect.h > 0.0f;
        cmd.clipRect = clipRect;
        cmd.vertices.assign(vertices, vertices + vertexCount);
        cmd.indices.assign(indices, indices + indexCount);
        m_geometryCommands.push_back(std::move(cmd));
    }

    //===========================
    //  GPU resources / states
    //===========================

    bool SpriteBatch2D::CreateShaders(ID3D11Device* device)
    {
        KBK_PROFILE_SCOPE("CreateBatchShaders");

        static constexpr const char* VS_SOURCE = R"(
cbuffer CB_VS : register(b0)
{
    float4x4 gViewProj;
};

struct VSInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color    : COLOR0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
    float4 color    : COLOR0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(float4(input.position, 1.0f), gViewProj);
    output.texcoord = input.texcoord;
    output.color    = input.color;
    return output;
}
)";

        static constexpr const char* PS_SOURCE = R"(
Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(float4 position : SV_Position,
            float2 texcoord : TEXCOORD0,
            float4 color    : COLOR0) : SV_Target
{
    float4 texColor = gTexture.Sample(gSampler, texcoord);
    return float4(texColor.rgb * color.rgb, texColor.a * color.a);
}
)";

        Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> errors;

        HRESULT hr = D3DCompile(
            VS_SOURCE, std::strlen(VS_SOURCE),
            nullptr, nullptr, nullptr,
            "main", "vs_5_0",
            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
            vsBlob.GetAddressOf(), errors.GetAddressOf());
        if (FAILED(hr)) {
            if (errors) {
                KbkError(kLogChannel, "VS compile error: %s",
                    static_cast<const char*>(errors->GetBufferPointer()));
            }
            return false;
        }

        errors.Reset();
        hr = D3DCompile(
            PS_SOURCE, std::strlen(PS_SOURCE),
            nullptr, nullptr, nullptr,
            "main", "ps_5_0",
            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
            psBlob.GetAddressOf(), errors.GetAddressOf());
        if (FAILED(hr)) {
            if (errors) {
                KbkError(kLogChannel, "PS compile error: %s",
                    static_cast<const char*>(errors->GetBufferPointer()));
            }
            return false;
        }

        hr = device->CreateVertexShader(
            vsBlob->GetBufferPointer(),
            vsBlob->GetBufferSize(),
            nullptr,
            m_vs.GetAddressOf());
        if (FAILED(hr)) {
            KbkError(kLogChannel, "CreateVertexShader failed: 0x%08X", static_cast<unsigned>(hr));
            return false;
        }

        hr = device->CreatePixelShader(
            psBlob->GetBufferPointer(),
            psBlob->GetBufferSize(),
            nullptr,
            m_ps.GetAddressOf());
        if (FAILED(hr)) {
            KbkError(kLogChannel, "CreatePixelShader failed: 0x%08X", static_cast<unsigned>(hr));
            return false;
        }

        // Describe the vertex input layout
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
              D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12,
              D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20,
              D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = device->CreateInputLayout(
            layout, ARRAYSIZE(layout),
            vsBlob->GetBufferPointer(),
            vsBlob->GetBufferSize(),
            m_inputLayout.GetAddressOf());
        if (FAILED(hr)) {
            KbkError(kLogChannel, "CreateInputLayout failed: 0x%08X", static_cast<unsigned>(hr));
            return false;
        }

        // Create the vertex shader constant buffer
        D3D11_BUFFER_DESC cbDesc{};
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbDesc.ByteWidth = sizeof(CBVS);

        hr = device->CreateBuffer(&cbDesc, nullptr, m_cbVS.GetAddressOf());
        if (FAILED(hr)) {
            KbkError(kLogChannel, "CreateBuffer (CB_VS) failed: 0x%08X", static_cast<unsigned>(hr));
            return false;
        }

        return true;
    }

    bool SpriteBatch2D::CreateStates(ID3D11Device* device)
    {
        KBK_PROFILE_SCOPE("CreateBatchStates");

        // Point-sampled sampler with clamp addressing
        D3D11_SAMPLER_DESC samp{};
        samp.AddressU = samp.AddressV = samp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samp.MinLOD = 0;
        samp.MaxLOD = D3D11_FLOAT32_MAX;
        samp.MaxAnisotropy = 1;
        samp.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        HRESULT hr = device->CreateSamplerState(&samp, m_samplerPoint.GetAddressOf());
        if (FAILED(hr)) {
            KbkError(kLogChannel, "CreateSamplerState failed: 0x%08X", static_cast<unsigned>(hr));
            return false;
        }

        // Standard alpha blending for sprites
        D3D11_BLEND_DESC blend{};
        blend.AlphaToCoverageEnable = FALSE;
        blend.IndependentBlendEnable = FALSE;
        blend.RenderTarget[0].BlendEnable = TRUE;
        blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        hr = device->CreateBlendState(&blend, m_blendAlpha.GetAddressOf());
        if (FAILED(hr)) {
            KbkError(kLogChannel, "CreateBlendState failed: 0x%08X", static_cast<unsigned>(hr));
            return false;
        }

        // Disable depth testing for 2D drawing
        D3D11_DEPTH_STENCIL_DESC depth{};
        depth.DepthEnable = FALSE;
        depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
        hr = device->CreateDepthStencilState(&depth, m_depthDisabled.GetAddressOf());
        if (FAILED(hr)) {
            KbkError(kLogChannel, "CreateDepthStencilState failed: 0x%08X", static_cast<unsigned>(hr));
            return false;
        }

        // Disable back-face culling for screen-aligned quads
        D3D11_RASTERIZER_DESC rast{};
        rast.FillMode = D3D11_FILL_SOLID;
        rast.CullMode = D3D11_CULL_NONE;
        rast.DepthClipEnable = TRUE;
        hr = device->CreateRasterizerState(&rast, m_rasterCullNone.GetAddressOf());
        if (FAILED(hr)) {
            KbkError(kLogChannel, "CreateRasterizerState failed: 0x%08X", static_cast<unsigned>(hr));
            return false;
        }

        rast.ScissorEnable = TRUE;
        hr = device->CreateRasterizerState(&rast, m_rasterCullNoneScissor.GetAddressOf());
        if (FAILED(hr)) {
            KbkError(kLogChannel, "CreateRasterizerState (scissor) failed: 0x%08X", static_cast<unsigned>(hr));
            return false;
        }

        return true;
    }

    bool SpriteBatch2D::EnsureVertexCapacity(size_t vertexCount)
    {
        KBK_PROFILE_SCOPE("EnsureVertexCapacity");

        if (vertexCount <= m_vertexCapacity && m_vertexBuffer)
            return true;

        size_t newCapacity = m_vertexCapacity == 0 ? 1024 : m_vertexCapacity;
        while (newCapacity < vertexCount)
            newCapacity *= 2;

        D3D11_BUFFER_DESC desc{};
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.ByteWidth = static_cast<UINT>(newCapacity * sizeof(Vertex));

        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
        const HRESULT hr = m_device->CreateBuffer(&desc, nullptr, buffer.GetAddressOf());
        if (FAILED(hr)) {
            KbkError(kLogChannel, "CreateBuffer (VB) failed: 0x%08X", static_cast<unsigned>(hr));
            return false;
        }

        m_vertexBuffer = buffer;
        m_vertexCapacity = newCapacity;
        return true;
    }

    bool SpriteBatch2D::EnsureIndexCapacity(size_t indexCount)
    {
        KBK_PROFILE_SCOPE("EnsureIndexCapacity");

        if (indexCount <= m_indexCapacity && m_indexBuffer)
            return true;

        size_t newCapacity = m_indexCapacity == 0 ? 2048 : m_indexCapacity;
        while (newCapacity < indexCount)
            newCapacity *= 2;

        D3D11_BUFFER_DESC desc{};
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.ByteWidth = static_cast<UINT>(newCapacity * sizeof(std::uint32_t));

        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
        const HRESULT hr = m_device->CreateBuffer(&desc, nullptr, buffer.GetAddressOf());
        if (FAILED(hr)) {
            KbkError(kLogChannel, "CreateBuffer (IB) failed: 0x%08X", static_cast<unsigned>(hr));
            return false;
        }

        m_indexBuffer = buffer;
        m_indexCapacity = newCapacity;
        return true;
    }

    void SpriteBatch2D::UpdateVSConstants()
    {
        KBK_PROFILE_SCOPE("UpdateBatchVSConstants");

        D3D11_MAPPED_SUBRESOURCE mapped{};
        const HRESULT hr = m_context->Map(m_cbVS.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(hr)) {
            KbkError(kLogChannel, "CB_VS map failed: 0x%08X", static_cast<unsigned>(hr));
            return;
        }

        auto* cb = reinterpret_cast<CBVS*>(mapped.pData);
        cb->viewProjT = m_viewProjT;

        m_context->Unmap(m_cbVS.Get(), 0);
    }

} // namespace KibakoEngine
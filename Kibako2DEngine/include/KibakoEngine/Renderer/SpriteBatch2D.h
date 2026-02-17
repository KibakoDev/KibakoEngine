// Batches sprites and UI geometry for Direct3D 11 rendering
#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <DirectXMath.h>

#include <cstdint>
#include <vector>

#include "KibakoEngine/Renderer/SpriteTypes.h"
#include "KibakoEngine/Renderer/Texture2D.h"

namespace KibakoEngine {

    struct SpriteBatchStats
    {
        std::uint32_t drawCalls = 0;
        std::uint32_t spritesSubmitted = 0; // counts only Push() sprite submissions
        std::uint32_t spritesCulled = 0;
    };

    class SpriteBatch2D {
    public:
        // Single vertex format shared by all 2D and UI geometry
        struct Vertex {
            DirectX::XMFLOAT3 position;
            DirectX::XMFLOAT2 uv;
            DirectX::XMFLOAT4 color;
        };

        // Lifetime management
        [[nodiscard]] bool Init(ID3D11Device* device, ID3D11DeviceContext* context);
        void Shutdown();

        // Frame boundaries
        void Begin(const DirectX::XMFLOAT4X4& viewProjT);
        void End();

        // Sprite submission helpers (positions are already in pixel space)
        void Push(const Texture2D& texture,
            const RectF& dst,
            const RectF& src,
            const Color4& color,
            float rotation = 0.0f,
            int layer = 0);

        // Raw geometry submission for UI
        // - texture can be nullptr to fall back to the built-in white texture
        // - vertices and indices are expected to be in screen space
        void PushGeometryRaw(const Texture2D* texture,
            const Vertex* vertices, size_t vertexCount,
            const std::uint32_t* indices, size_t indexCount,
            int layer = 0,
            const RectF& clipRect = RectF::FromXYWH(0.0f, 0.0f, 0.0f, 0.0f));

        // Zero-copy path for long-lived geometry (compiled UI meshes, static overlays)
        void PushGeometryView(const Texture2D* texture,
            const Vertex* vertices,
            size_t vertexCount,
            const std::uint32_t* indices,
            size_t indexCount,
            int layer = 0,
            const RectF& clipRect = RectF::FromXYWH(0.0f, 0.0f, 0.0f, 0.0f),
            DirectX::XMFLOAT2 translation = { 0.0f, 0.0f });

        void ResetStats() { m_stats = {}; }
        void RecordSpriteCulled() { ++m_stats.spritesCulled; }
        [[nodiscard]] const SpriteBatchStats& Stats() const { return m_stats; }

        [[nodiscard]] const Texture2D* DefaultWhiteTexture() const;

    private:
        // Logical sprite command built from a quad
        struct DrawCommand {
            const Texture2D* texture = nullptr;
            RectF  dst;
            RectF  src;
            Color4 color;
            float  rotation = 0.0f;
            int    layer = 0;
        };

        // Raw geometry command used by UI / Rml
        struct GeometryCommand {
            const Texture2D* texture = nullptr;
            const Vertex* vertices = nullptr;
            const std::uint32_t* indices = nullptr;
            size_t                   vertexCount = 0;
            size_t                   indexCount = 0;
            std::vector<Vertex>      ownedVertices;
            std::vector<std::uint32_t> ownedIndices;
            bool hasTranslation = false;
            DirectX::XMFLOAT2 translation{ 0.0f, 0.0f };
            int layer = 0;
            bool hasClipRect = false;
            RectF clipRect{};
        };

        struct CBVS {
            DirectX::XMFLOAT4X4 viewProjT;
        };

        // Final draw range consumed by DrawIndexed
        struct DrawRange {
            ID3D11ShaderResourceView* srv = nullptr;
            int              layer = 0;
            std::uint32_t    firstIndex = 0;
            std::uint32_t    indexCount = 0;
            bool             useScissor = false;
            D3D11_RECT       scissorRect{ 0, 0, 0, 0 };
        };

        struct UnifiedCommand {
            const Texture2D* texture = nullptr;
            ID3D11ShaderResourceView* srv = nullptr;
            int              layer = 0;
            bool             isSprite = true;
            size_t           index = 0;
        };

        [[nodiscard]] bool CreateShaders(ID3D11Device* device);
        [[nodiscard]] bool CreateStates(ID3D11Device* device);

        [[nodiscard]] bool EnsureVertexCapacity(size_t vertexCount);
        [[nodiscard]] bool EnsureIndexCapacity(size_t indexCount);

        void UpdateVSConstants();
        void ClearFrameData();

        // GPU resources and fixed states
        ID3D11Device* m_device = nullptr;
        ID3D11DeviceContext* m_context = nullptr;

        Microsoft::WRL::ComPtr<ID3D11VertexShader>      m_vs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_ps;
        Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_inputLayout;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_indexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_cbVS;
        Microsoft::WRL::ComPtr<ID3D11SamplerState>      m_samplerPoint;
        Microsoft::WRL::ComPtr<ID3D11BlendState>        m_blendAlpha;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthDisabled;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_rasterCullNone;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_rasterCullNoneScissor;

        // Logical commands collected during a frame
        std::vector<DrawCommand>     m_commands;         // sprite quads
        std::vector<GeometryCommand> m_geometryCommands; // raw geometry

        // Batching helpers reused each frame
        std::vector<UnifiedCommand>  m_unifiedCommands;
        std::vector<DrawRange>       m_drawRanges;

        size_t m_spriteReserveHint = 256;
        size_t m_geometryReserveHint = 256;

        DirectX::XMFLOAT4X4 m_viewProjT{};
        size_t              m_vertexCapacity = 0; // number of vertices allocated
        size_t              m_indexCapacity = 0; // number of indices allocated
        bool                m_isDrawing = false;

        SpriteBatchStats    m_stats{};

        Texture2D m_defaultWhite;
    };

} // namespace KibakoEngine
// RenderInterface implementation that sends RmlUI draw calls to the sprite batch
#pragma once

#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/Types.h>
#include <RmlUi/Core/Span.h>

#include <unordered_map>
#include <vector>

#include "KibakoEngine/Renderer/SpriteBatch2D.h"
#include "KibakoEngine/Renderer/Texture2D.h"

namespace KibakoEngine {

    class RendererD3D11;

    class RmlRenderInterfaceD3D11 : public Rml::RenderInterface {
    public:
        RmlRenderInterfaceD3D11(RendererD3D11& renderer, int uiWidth, int uiHeight);
        ~RmlRenderInterfaceD3D11() override = default;

        // RmlUI rendering API
        Rml::CompiledGeometryHandle CompileGeometry(
            Rml::Span<const Rml::Vertex> vertices,
            Rml::Span<const int> indices) override;

        void RenderGeometry(
            Rml::CompiledGeometryHandle geometry,
            Rml::Vector2f translation,
            Rml::TextureHandle texture) override;

        void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

        Rml::TextureHandle LoadTexture(
            Rml::Vector2i& dimensions,
            const Rml::String& source) override;

        Rml::TextureHandle GenerateTexture(
            Rml::Span<const Rml::byte> source,
            Rml::Vector2i dimensions) override;

        void ReleaseTexture(Rml::TextureHandle texture) override;

        // Viewport / UI surface size in pixels (window resolution)
        void SetViewportSize(int uiWidth, int uiHeight)
        {
            m_uiWidth = (uiWidth > 0) ? uiWidth : 1;
            m_uiHeight = (uiHeight > 0) ? uiHeight : 1;
        }

        // Transform / scissor are ignored; UI vertices are emitted directly in pixel space.
        void EnableScissorRegion(bool enable) override { (void)enable; }
        void SetScissorRegion(Rml::Rectanglei region) override { (void)region; }
        void SetTransform(const Rml::Matrix4f* transform) override { (void)transform; }

    private:
        RendererD3D11& m_Renderer;

        struct GeometryBlock {
            std::vector<SpriteBatch2D::Vertex> vertices;
            std::vector<std::uint32_t>         indices;
        };

        std::unordered_map<Rml::CompiledGeometryHandle, GeometryBlock> m_Geometry;
        Rml::CompiledGeometryHandle m_GeometryCounter = 1;

        std::unordered_map<Rml::TextureHandle, Texture2D> m_Textures;
        Rml::TextureHandle m_TextureCounter = 1;

        std::vector<SpriteBatch2D::Vertex> m_transformedVertices;

        int m_uiWidth = 1; // in pixels (window space)
        int m_uiHeight = 1;
    };

} // namespace KibakoEngine

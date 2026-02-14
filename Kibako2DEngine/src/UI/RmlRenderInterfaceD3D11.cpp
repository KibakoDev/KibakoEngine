// Bridges RmlUI drawing requests to SpriteBatch2D and Direct3D 11
#include "KibakoEngine/UI/RmlRenderInterfaceD3D11.h"

#include "KibakoEngine/Renderer/RendererD3D11.h"
#include "KibakoEngine/Core/Log.h"

#include <algorithm>
#include <cmath>

namespace KibakoEngine {

    namespace {
        constexpr const char* kLog = "RmlRender";

        static RectF BuildClipRect(const Rml::Rectanglei& region)
        {
            const float left = static_cast<float>(region.Left());
            const float top = static_cast<float>(region.Top());
            const float width = static_cast<float>(std::max(0, region.Width()));
            const float height = static_cast<float>(std::max(0, region.Height()));
            return RectF::FromXYWH(left, top, width, height);
        }
    }

    //-------------------------------------
    // Construction
    //-------------------------------------

    RmlRenderInterfaceD3D11::RmlRenderInterfaceD3D11(RendererD3D11& renderer,
        int uiWidth,
        int uiHeight)
        : m_Renderer(renderer)
    {
        SetViewportSize(uiWidth, uiHeight);
    }

    //-------------------------------------
    // CompileGeometry
    //-------------------------------------

    Rml::CompiledGeometryHandle RmlRenderInterfaceD3D11::CompileGeometry(
        Rml::Span<const Rml::Vertex> vertices,
        Rml::Span<const int> indices)
    {
        GeometryBlock block;
        block.vertices.reserve(vertices.size());
        block.indices.reserve(indices.size());

        for (const Rml::Vertex& v : vertices) {
            SpriteBatch2D::Vertex out{};

            // Positions are in UI space (0..uiWidth / 0..uiHeight)
            out.position = { v.position.x, v.position.y, 0.0f };

            // Copy UV coordinates as-is
            out.uv = { v.tex_coord.x, v.tex_coord.y };

            // Convert vertex colour (0-255) to floats
            out.color = {
                v.colour.red / 255.0f,
                v.colour.green / 255.0f,
                v.colour.blue / 255.0f,
                v.colour.alpha / 255.0f
            };

            block.vertices.push_back(out);
        }

        for (int i : indices)
            block.indices.push_back(static_cast<std::uint32_t>(i));

        Rml::CompiledGeometryHandle handle = m_GeometryCounter++;
        m_Geometry[handle] = std::move(block);
        return handle;
    }

    //-------------------------------------
    // RenderGeometry
    //-------------------------------------

    void RmlRenderInterfaceD3D11::RenderGeometry(
        Rml::CompiledGeometryHandle geometry,
        Rml::Vector2f translation,
        Rml::TextureHandle texture)
    {
        auto it = m_Geometry.find(geometry);
        if (it == m_Geometry.end())
            return;

        GeometryBlock& geo = it->second;

        const bool needsTranslation =
            std::fabs(translation.x) > 0.0f || std::fabs(translation.y) > 0.0f;

        const SpriteBatch2D::Vertex* vertices = geo.vertices.data();
        size_t vertexCount = geo.vertices.size();
        if (needsTranslation) {
            m_transformedVertices.resize(geo.vertices.size());
            for (std::size_t i = 0; i < geo.vertices.size(); ++i) {
                SpriteBatch2D::Vertex v = geo.vertices[i];
                v.position.x += translation.x;
                v.position.y += translation.y;
                m_transformedVertices[i] = v;
            }
            vertices = m_transformedVertices.data();
            vertexCount = m_transformedVertices.size();
        }

        const Texture2D* texPtr = nullptr;
        auto texIt = m_Textures.find(texture);
        if (texIt != m_Textures.end()) {
            texPtr = &texIt->second;
        }
        else {
            texPtr = m_Renderer.Batch().DefaultWhiteTexture();
        }

        if (m_scissorEnabled) {
            m_Renderer.Batch().PushGeometryRaw(
                texPtr,
                vertices, vertexCount,
                geo.indices.data(), geo.indices.size(),
                100000,
                BuildClipRect(m_scissorRegion)
            );
            return;
        }

        m_Renderer.Batch().PushGeometryRaw(
            texPtr,
            vertices, vertexCount,
            geo.indices.data(), geo.indices.size(),
            100000
        );
    }

    void RmlRenderInterfaceD3D11::EnableScissorRegion(bool enable)
    {
        m_scissorEnabled = enable;
    }

    void RmlRenderInterfaceD3D11::SetScissorRegion(Rml::Rectanglei region)
    {
        m_scissorRegion = region;
    }

    //-------------------------------------
    // ReleaseGeometry
    //-------------------------------------

    void RmlRenderInterfaceD3D11::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
    {
        m_Geometry.erase(geometry);
    }

    //-------------------------------------
    // LoadTexture
    //-------------------------------------

    Rml::TextureHandle RmlRenderInterfaceD3D11::LoadTexture(
        Rml::Vector2i& dimensions,
        const Rml::String& source)
    {
        Texture2D tex;

        if (!tex.LoadFromFile(m_Renderer.GetDevice(), source.c_str(), false)) {
            KbkWarn(kLog, "Failed to load Rml texture: %s", source.c_str());
            return 0;
        }

        dimensions.x = tex.Width();
        dimensions.y = tex.Height();

        Rml::TextureHandle handle = m_TextureCounter++;
        m_Textures[handle] = std::move(tex);
        return handle;
    }

    //-------------------------------------
    // GenerateTexture
    //-------------------------------------

    Rml::TextureHandle RmlRenderInterfaceD3D11::GenerateTexture(
        Rml::Span<const Rml::byte> source,
        Rml::Vector2i dimensions)
    {
        Texture2D tex;

        if (!tex.CreateFromRGBA8(
            m_Renderer.GetDevice(),
            dimensions.x, dimensions.y,
            reinterpret_cast<const std::uint8_t*>(source.data()))) {
            KbkWarn(kLog, "Failed to generate Rml texture");
            return 0;
        }

        Rml::TextureHandle handle = m_TextureCounter++;
        m_Textures[handle] = std::move(tex);
        return handle;
    }

    //-------------------------------------
    // ReleaseTexture
    //-------------------------------------

    void RmlRenderInterfaceD3D11::ReleaseTexture(Rml::TextureHandle texture)
    {
        m_Textures.erase(texture);
    }

} // namespace KibakoEngine
// Simple orthographic camera controller for 1:1 pixel-aligned 2D scenes
#pragma once

#include <DirectXMath.h>

namespace KibakoEngine {

    class Camera2D {
    public:
        void SetViewport(float width, float height);
        void SetPosition(float x, float y);
        void SetRotation(float radians);

        [[nodiscard]] DirectX::XMFLOAT2 GetPosition() const { return { m_positionX, m_positionY }; }
        [[nodiscard]] float             GetRotation() const { return m_rotation; }
        [[nodiscard]] float             GetViewportWidth() const { return m_viewWidth; }
        [[nodiscard]] float             GetViewportHeight() const { return m_viewHeight; }
        [[nodiscard]] DirectX::XMFLOAT4X4 GetViewProjection() const { return m_viewProj; }
        [[nodiscard]] const DirectX::XMFLOAT4X4& GetViewProjectionT() const { return m_viewProjT; }

    private:
        void UpdateMatrix();

        float m_viewWidth = 1.0f;
        float m_viewHeight = 1.0f;
        float m_positionX = 0.0f;
        float m_positionY = 0.0f;
        float m_rotation = 0.0f;

        DirectX::XMFLOAT4X4 m_viewProj{};
        DirectX::XMFLOAT4X4 m_viewProjT{};
    };

} // namespace KibakoEngine


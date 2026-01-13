// Lightweight 2D scene container with sprites and optional colliders
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <deque>

#include <DirectXMath.h>

#include "KibakoEngine/Renderer/SpriteTypes.h"
#include "KibakoEngine/Renderer/Texture2D.h"
#include "KibakoEngine/Collision/Collision2D.h"

namespace KibakoEngine {

    class SpriteBatch2D;
    class AssetManager;

    using EntityID = std::uint32_t;

    struct Transform2D
    {
        DirectX::XMFLOAT2 position{ 0.0f, 0.0f };
        float              rotation = 0.0f;
        DirectX::XMFLOAT2  scale{ 1.0f, 1.0f };
    };

    struct SpriteRenderer2D
    {
        // ---- Data-driven fields (Phase 1)
        std::string textureId;
        std::string texturePath;
        bool        textureSRGB = true;

        // ---- Runtime cache
        Texture2D* texture = nullptr;

        RectF  dst{ 0.0f, 0.0f, 0.0f, 0.0f };
        RectF  src{ 0.0f, 0.0f, 1.0f, 1.0f };
        Color4 color = Color4::White();
        int    layer = 0;
    };

    struct Entity2D
    {
        EntityID id = 0;
        bool     active = true;

        Transform2D          transform;
        SpriteRenderer2D     sprite;
        CollisionComponent2D collision;
    };

    class Scene2D
    {
    public:
        Scene2D() = default;

        [[nodiscard]] Entity2D& CreateEntity();
        void                   DestroyEntity(EntityID id);

        void Clear();

        [[nodiscard]] Entity2D* FindEntity(EntityID id);
        [[nodiscard]] const Entity2D* FindEntity(EntityID id) const;

        std::vector<Entity2D>& Entities() { return m_entities; }
        const std::vector<Entity2D>& Entities() const { return m_entities; }

        void Update(float dt);
        void Render(SpriteBatch2D& batch) const;

        // ---- Phase 1 API (stubs for now)
        bool LoadFromFile(const char* path, AssetManager& assets);
        void ResolveAssets(AssetManager& assets);

        // ---- Helpers (used by sandbox & later by loader)
        CircleCollider2D* AddCircleCollider(Entity2D& e, float radius, bool active = true);
        AABBCollider2D* AddAABBCollider(Entity2D& e, float halfW, float halfH, bool active = true);

    private:
        EntityID m_nextID = 1;
        std::vector<Entity2D> m_entities;

        // Scene-owned collider storage (stable pointers)
        std::deque<CircleCollider2D> m_circlePool;
        std::deque<AABBCollider2D>   m_aabbPool;
    };

} // namespace KibakoEngine
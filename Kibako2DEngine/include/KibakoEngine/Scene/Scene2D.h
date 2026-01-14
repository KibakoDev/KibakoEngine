// Lightweight 2D scene container with component stores (Phase 2)
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <deque>

#include <DirectXMath.h>

#include "KibakoEngine/Renderer/SpriteTypes.h"
#include "KibakoEngine/Renderer/Texture2D.h"
#include "KibakoEngine/Collision/Collision2D.h"
#include "KibakoEngine/Scene/ComponentStore.h"

namespace KibakoEngine {

    class SpriteBatch2D;
    class AssetManager;

    struct Transform2D
    {
        DirectX::XMFLOAT2 position{ 0.0f, 0.0f };
        float             rotation = 0.0f;
        DirectX::XMFLOAT2 scale{ 1.0f, 1.0f };
    };

    // Component: Sprite renderer (data-driven + runtime cached pointer)
    struct SpriteRenderer2D
    {
        // Data-driven fields
        std::string textureId;
        std::string texturePath;
        bool        textureSRGB = true;

        // Runtime cache
        Texture2D* texture = nullptr;

        RectF  dst{ 0.0f, 0.0f, 0.0f, 0.0f };
        RectF  src{ 0.0f, 0.0f, 1.0f, 1.0f };
        Color4 color = Color4::White();
        int    layer = 0;
    };

    // Entity: minimal, stable, solo-dev friendly
    struct Entity2D
    {
        EntityID id = 0;
        bool     active = true;

        // Always-on component
        Transform2D transform;
    };

    class Scene2D
    {
    public:
        Scene2D() = default;

        [[nodiscard]] Entity2D& CreateEntity();
        [[nodiscard]] Entity2D& CreateEntityWithID(EntityID forcedId);

        void DestroyEntity(EntityID id);
        void Clear();

        [[nodiscard]] Entity2D* FindEntity(EntityID id);
        [[nodiscard]] const Entity2D* FindEntity(EntityID id) const;

        std::vector<Entity2D>& Entities() { return m_entities; }
        const std::vector<Entity2D>& Entities() const { return m_entities; }

        // Component stores access
        [[nodiscard]] ComponentStore<SpriteRenderer2D>& Sprites() { return m_sprites; }
        [[nodiscard]] const ComponentStore<SpriteRenderer2D>& Sprites() const { return m_sprites; }

        [[nodiscard]] ComponentStore<CollisionComponent2D>& Collisions() { return m_collisions; }
        [[nodiscard]] const ComponentStore<CollisionComponent2D>& Collisions() const { return m_collisions; }

        // Component helpers
        [[nodiscard]] SpriteRenderer2D& AddSprite(EntityID id);

        CircleCollider2D* AddCircleCollider(EntityID id, float radius, bool active = true);
        AABBCollider2D* AddAABBCollider(EntityID id, float halfW, float halfH, bool active = true);

        void Update(float dt);
        void Render(SpriteBatch2D& batch) const;

        bool LoadFromFile(const char* path, AssetManager& assets);
        void ResolveAssets(AssetManager& assets);

    private:
        EntityID m_nextID = 1;
        std::vector<Entity2D> m_entities;

        // Sparse-set stores (perf-ready)
        ComponentStore<SpriteRenderer2D>     m_sprites;
        ComponentStore<CollisionComponent2D> m_collisions;

        // Scene-owned collider pools (stable pointers)
        std::deque<CircleCollider2D> m_circlePool;
        std::deque<AABBCollider2D>   m_aabbPool;
    };

} // namespace KibakoEngine
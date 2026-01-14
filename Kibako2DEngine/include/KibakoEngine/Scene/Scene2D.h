// Lightweight 2D scene container with component stores (Phase 3A)
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

    // ---- Components ---------------------------------------------------------

    struct SpriteRenderer2D
    {
        std::string textureId;
        std::string texturePath;
        bool        textureSRGB = true;

        Texture2D* texture = nullptr;

        RectF  dst{ 0.0f, 0.0f, 0.0f, 0.0f };
        RectF  src{ 0.0f, 0.0f, 1.0f, 1.0f };
        Color4 color = Color4::White();
        int    layer = 0;
    };

    struct NameComponent
    {
        std::string name;
    };

    // ---- Entity -------------------------------------------------------------

    struct Entity2D
    {
        EntityID id = 0;
        bool     active = true;

        Transform2D transform;
    };

    // ---- Scene --------------------------------------------------------------

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

        [[nodiscard]] Entity2D* FindByName(const std::string& name);
        [[nodiscard]] const Entity2D* FindByName(const std::string& name) const;

        std::vector<Entity2D>& Entities() { return m_entities; }
        const std::vector<Entity2D>& Entities() const { return m_entities; }

        // ---- Component stores access ---------------------------------------

        ComponentStore<SpriteRenderer2D>& Sprites() { return m_sprites; }
        ComponentStore<CollisionComponent2D>& Collisions() { return m_collisions; }
        ComponentStore<NameComponent>& Names() { return m_names; }

        const ComponentStore<SpriteRenderer2D>& Sprites() const { return m_sprites; }
        const ComponentStore<CollisionComponent2D>& Collisions() const { return m_collisions; }
        const ComponentStore<NameComponent>& Names() const { return m_names; }

        // ---- Component helpers ---------------------------------------------

        SpriteRenderer2D& AddSprite(EntityID id);

        NameComponent& AddName(EntityID id, const std::string& name = {});
        NameComponent* TryGetName(EntityID id);
        const NameComponent* TryGetName(EntityID id) const;

        CircleCollider2D* AddCircleCollider(EntityID id, float radius, bool active = true);
        AABBCollider2D* AddAABBCollider(EntityID id, float halfW, float halfH, bool active = true);

        // ---- Runtime --------------------------------------------------------

        void Update(float dt);
        void Render(SpriteBatch2D& batch) const;

        [[nodiscard]] bool LoadFromFile(const char* path, AssetManager& assets);
        void ResolveAssets(AssetManager& assets);

    private:
        EntityID m_nextID = 1;
        std::vector<Entity2D> m_entities;

        ComponentStore<SpriteRenderer2D>      m_sprites;
        ComponentStore<CollisionComponent2D> m_collisions;
        ComponentStore<NameComponent>        m_names;

        std::deque<CircleCollider2D> m_circlePool;
        std::deque<AABBCollider2D>   m_aabbPool;
    };

} // namespace KibakoEngine

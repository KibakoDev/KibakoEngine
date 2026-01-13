// Stores and renders collections of 2D entities
#include "KibakoEngine/Scene/Scene2D.h"

#include "KibakoEngine/Core/Debug.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/Renderer/SpriteBatch2D.h"

#include <algorithm>

namespace KibakoEngine {

    namespace
    {
        constexpr const char* kLogChannel = "Scene2D";

        template <typename Container>
        auto FindEntityIt(Container& entities, EntityID id)
        {
            return std::find_if(entities.begin(), entities.end(), [id](const Entity2D& entity) {
                return entity.id == id;
                });
        }
    }

    Entity2D& Scene2D::CreateEntity()
    {
        Entity2D& entity = m_entities.emplace_back();
        entity.id = m_nextID++;
        entity.active = true;

        KbkTrace(kLogChannel, "Created Entity2D id=%u", entity.id);
        return entity;
    }

    void Scene2D::DestroyEntity(EntityID id)
    {
        const auto it = FindEntityIt(m_entities, id);
        if (it == m_entities.end())
            return;

        it->active = false;
        KbkTrace(kLogChannel, "Destroyed Entity2D id=%u (marked inactive)", id);
    }

    void Scene2D::Clear()
    {
        m_entities.clear();
        m_circlePool.clear();
        m_aabbPool.clear();
        m_nextID = 1;

        KbkLog(kLogChannel, "Scene2D cleared");
    }

    Entity2D* Scene2D::FindEntity(EntityID id)
    {
        const auto it = FindEntityIt(m_entities, id);
        return it != m_entities.end() ? &(*it) : nullptr;
    }

    const Entity2D* Scene2D::FindEntity(EntityID id) const
    {
        const auto it = FindEntityIt(m_entities, id);
        return it != m_entities.end() ? &(*it) : nullptr;
    }

    void Scene2D::Update(float dt)
    {
        KBK_UNUSED(dt);
    }

    void Scene2D::Render(SpriteBatch2D& batch) const
    {
        for (const auto& entity : m_entities) {
            if (!entity.active)
                continue;

            const Texture2D* texture = entity.sprite.texture;
            if (!texture || !texture->IsValid())
                continue;

            const RectF& local = entity.sprite.dst;
            const Transform2D& transform = entity.transform;

            const float scaledWidth = local.w * transform.scale.x;
            const float scaledHeight = local.h * transform.scale.y;

            const float offsetX = local.x * transform.scale.x;
            const float offsetY = local.y * transform.scale.y;

            const float worldCenterX = transform.position.x + offsetX;
            const float worldCenterY = transform.position.y + offsetY;

            RectF dst{};
            dst.w = scaledWidth;
            dst.h = scaledHeight;
            dst.x = worldCenterX - (scaledWidth * 0.5f);
            dst.y = worldCenterY - (scaledHeight * 0.5f);

            batch.Push(
                *texture,
                dst,
                entity.sprite.src,
                entity.sprite.color,
                transform.rotation,
                entity.sprite.layer);
        }
    }

    // ---- Phase 1 stubs
    bool Scene2D::LoadFromFile(const char* path, AssetManager& assets)
    {
        KBK_UNUSED(path);
        KBK_UNUSED(assets);
        return false;
    }

    void Scene2D::ResolveAssets(AssetManager& assets)
    {
        KBK_UNUSED(assets);
    }

    // ---- Collider helpers
    CircleCollider2D* Scene2D::AddCircleCollider(Entity2D& e, float radius, bool active)
    {
        auto& c = m_circlePool.emplace_back();
        c.radius = radius;
        c.active = active;

        e.collision.circle = &c;
        e.collision.aabb = nullptr;
        return &c;
    }

    AABBCollider2D* Scene2D::AddAABBCollider(Entity2D& e, float halfW, float halfH, bool active)
    {
        auto& b = m_aabbPool.emplace_back();
        b.halfW = halfW;
        b.halfH = halfH;
        b.active = active;

        e.collision.aabb = &b;
        e.collision.circle = nullptr;
        return &b;
    }

} // namespace KibakoEngine
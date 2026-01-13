// Stores and renders collections of 2D entities (Phase 2: component stores)
#include "KibakoEngine/Scene/Scene2D.h"

#include "KibakoEngine/Core/Debug.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/Renderer/SpriteBatch2D.h"
#include "KibakoEngine/Resources/AssetManager.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace KibakoEngine {

    namespace
    {
        constexpr const char* kLogChannel = "Scene2D";

        template <typename Container>
        auto FindEntityIt(Container& entities, EntityID id)
        {
            return std::find_if(entities.begin(), entities.end(),
                [id](const Entity2D& entity) { return entity.id == id; });
        }

        static std::string ReadAllText(const char* path)
        {
            std::ifstream file(path, std::ios::in | std::ios::binary);
            if (!file)
                return {};
            std::ostringstream ss;
            ss << file.rdbuf();
            return ss.str();
        }

        static DirectX::XMFLOAT2 ReadVec2(const nlohmann::json& arr, float defX, float defY)
        {
            if (!arr.is_array() || arr.size() < 2)
                return { defX, defY };
            return { arr[0].get<float>(), arr[1].get<float>() };
        }

        static RectF ReadRectF(const nlohmann::json& arr, const RectF& def)
        {
            if (!arr.is_array() || arr.size() < 4)
                return def;
            return RectF::FromXYWH(
                arr[0].get<float>(),
                arr[1].get<float>(),
                arr[2].get<float>(),
                arr[3].get<float>());
        }

        static Color4 ReadColor4(const nlohmann::json& arr, const Color4& def)
        {
            if (!arr.is_array() || arr.size() < 4)
                return def;
            return Color4{
                arr[0].get<float>(),
                arr[1].get<float>(),
                arr[2].get<float>(),
                arr[3].get<float>()
            };
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

    Entity2D& Scene2D::CreateEntityWithID(EntityID forcedId)
    {
        Entity2D& entity = m_entities.emplace_back();
        entity.id = forcedId;
        entity.active = true;

        if (forcedId >= m_nextID)
            m_nextID = forcedId + 1;

        KbkTrace(kLogChannel, "Created Entity2D (forced) id=%u", entity.id);
        return entity;
    }

    void Scene2D::DestroyEntity(EntityID id)
    {
        const auto it = FindEntityIt(m_entities, id);
        if (it == m_entities.end())
            return;

        it->active = false;

        // Remove optional components
        m_sprites.Remove(id);
        m_collisions.Remove(id);

        KbkTrace(kLogChannel, "Destroyed Entity2D id=%u (marked inactive)", id);
    }

    void Scene2D::Clear()
    {
        m_entities.clear();

        m_sprites.Clear();
        m_collisions.Clear();

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
        // gameplay/systems will live elsewhere later
    }

    SpriteRenderer2D& Scene2D::AddSprite(EntityID id)
    {
        return m_sprites.Add(id);
    }

    CircleCollider2D* Scene2D::AddCircleCollider(EntityID id, float radius, bool active)
    {
        auto& c = m_circlePool.emplace_back();
        c.radius = radius;
        c.active = active;

        auto& comp = m_collisions.Add(id);
        comp.circle = &c;
        comp.aabb = nullptr;

        return &c;
    }

    AABBCollider2D* Scene2D::AddAABBCollider(EntityID id, float halfW, float halfH, bool active)
    {
        auto& b = m_aabbPool.emplace_back();
        b.halfW = halfW;
        b.halfH = halfH;
        b.active = active;

        auto& comp = m_collisions.Add(id);
        comp.aabb = &b;
        comp.circle = nullptr;

        return &b;
    }

    void Scene2D::Render(SpriteBatch2D& batch) const
    {
        for (const auto& entity : m_entities) {
            if (!entity.active)
                continue;

            const SpriteRenderer2D* spr = m_sprites.TryGet(entity.id);
            if (!spr)
                continue;

            const Texture2D* texture = spr->texture;
            if (!texture || !texture->IsValid())
                continue;

            const RectF& local = spr->dst;
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
                spr->src,
                spr->color,
                transform.rotation,
                spr->layer);
        }
    }

    bool Scene2D::LoadFromFile(const char* path, AssetManager& assets)
    {
        if (!path || path[0] == '\0') {
            KbkError(kLogChannel, "LoadFromFile: empty path");
            return false;
        }

        const std::string text = ReadAllText(path);
        if (text.empty()) {
            KbkError(kLogChannel, "LoadFromFile: failed to read '%s'", path);
            return false;
        }

        nlohmann::json root;
        try {
            root = nlohmann::json::parse(text);
        }
        catch (const std::exception& e) {
            KbkError(kLogChannel, "LoadFromFile: JSON parse error in '%s': %s", path, e.what());
            return false;
        }

        Clear();

        const auto itEntities = root.find("entities");
        if (itEntities == root.end() || !itEntities->is_array()) {
            KbkWarn(kLogChannel, "LoadFromFile: no 'entities' array in '%s'", path);
            return true;
        }

        const auto& entitiesJson = *itEntities;
        m_entities.reserve(entitiesJson.size());

        for (const auto& eJson : entitiesJson)
        {
            EntityID forcedId = 0;
            if (eJson.contains("id"))
                forcedId = eJson["id"].get<EntityID>();

            Entity2D& e = (forcedId != 0) ? CreateEntityWithID(forcedId) : CreateEntity();

            // active
            if (eJson.contains("active"))
                e.active = eJson["active"].get<bool>();

            // transform
            if (eJson.contains("transform")) {
                const auto& t = eJson["transform"];
                if (t.contains("pos"))
                    e.transform.position = ReadVec2(t["pos"], 0.0f, 0.0f);
                if (t.contains("rot"))
                    e.transform.rotation = t["rot"].get<float>();
                if (t.contains("scale"))
                    e.transform.scale = ReadVec2(t["scale"], 1.0f, 1.0f);
            }

            // sprite component
            if (eJson.contains("sprite")) {
                auto& spr = AddSprite(e.id);
                const auto& s = eJson["sprite"];

                if (s.contains("texture")) {
                    const auto& tex = s["texture"];
                    if (tex.contains("id"))   spr.textureId = tex["id"].get<std::string>();
                    if (tex.contains("path")) spr.texturePath = tex["path"].get<std::string>();
                    if (tex.contains("sRGB")) spr.textureSRGB = tex["sRGB"].get<bool>();
                }

                if (s.contains("dst"))   spr.dst = ReadRectF(s["dst"], spr.dst);
                if (s.contains("src"))   spr.src = ReadRectF(s["src"], spr.src);
                if (s.contains("color")) spr.color = ReadColor4(s["color"], spr.color);
                if (s.contains("layer")) spr.layer = s["layer"].get<int>();
            }

            // collision component
            if (eJson.contains("collision") && !eJson["collision"].is_null()) {
                const auto& c = eJson["collision"];
                if (c.contains("type")) {
                    const std::string type = c["type"].get<std::string>();
                    const bool active = c.contains("active") ? c["active"].get<bool>() : true;

                    if (type == "circle") {
                        const float radius = c.contains("radius") ? c["radius"].get<float>() : 0.0f;
                        AddCircleCollider(e.id, radius, active);
                    }
                    else if (type == "aabb") {
                        const float halfW = c.contains("halfW") ? c["halfW"].get<float>() : 0.0f;
                        const float halfH = c.contains("halfH") ? c["halfH"].get<float>() : 0.0f;
                        AddAABBCollider(e.id, halfW, halfH, active);
                    }
                }
            }
        }

        ResolveAssets(assets);

        KbkLog(kLogChannel, "Loaded scene '%s' (%zu entities)", path, m_entities.size());
        return true;
    }

    void Scene2D::ResolveAssets(AssetManager& assets)
    {
        m_sprites.ForEach([&](EntityID, SpriteRenderer2D& spr)
            {
                if (!spr.texture &&
                    !spr.textureId.empty() &&
                    !spr.texturePath.empty()) {

                    spr.texture = assets.LoadTexture(
                        spr.textureId,
                        spr.texturePath,
                        spr.textureSRGB);
                }
            });
    }

} // namespace KibakoEngine
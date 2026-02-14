// Stores and renders collections of 2D entities (Phase 3A)
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
                [id](const Entity2D& e) { return e.id == id; });
        }

        std::string ReadAllText(const char* path)
        {
            if (!path || path[0] == '\0')
                return {};

            std::ifstream file(path, std::ios::binary);
            if (!file)
                return {};

            std::ostringstream ss;
            ss << file.rdbuf();
            return ss.str();
        }

        DirectX::XMFLOAT2 ReadVec2(const nlohmann::json& arr, float dx, float dy)
        {
            if (!arr.is_array() || arr.size() < 2)
                return { dx, dy };

            // "get<float>()" can throw if type mismatch; value() is safer but doesn't work on arrays.
            // We keep get<float>() but guard with is_number().
            const auto& x = arr[0];
            const auto& y = arr[1];

            if (!x.is_number() || !y.is_number())
                return { dx, dy };

            return { x.get<float>(), y.get<float>() };
        }

        RectF ReadRectF(const nlohmann::json& arr, const RectF& def)
        {
            if (!arr.is_array() || arr.size() < 4)
                return def;

            for (int i = 0; i < 4; ++i)
                if (!arr[i].is_number())
                    return def;

            return RectF::FromXYWH(
                arr[0].get<float>(),
                arr[1].get<float>(),
                arr[2].get<float>(),
                arr[3].get<float>());
        }

        Color4 ReadColor4(const nlohmann::json& arr, const Color4& def)
        {
            if (!arr.is_array() || arr.size() < 4)
                return def;

            for (int i = 0; i < 4; ++i)
                if (!arr[i].is_number())
                    return def;

            return {
                arr[0].get<float>(),
                arr[1].get<float>(),
                arr[2].get<float>(),
                arr[3].get<float>()
            };
        }
    } // namespace

    // ------------------------------------------------------------------------

    Entity2D& Scene2D::CreateEntity()
    {
        Entity2D& e = m_entities.emplace_back();
        e.id = m_nextID++;
        e.active = true;
        return e;
    }

    Entity2D& Scene2D::CreateEntityWithID(EntityID forcedId)
    {
        Entity2D& e = m_entities.emplace_back();
        e.id = forcedId;
        e.active = true;

        if (forcedId >= m_nextID)
            m_nextID = forcedId + 1;

        return e;
    }

    void Scene2D::DestroyEntity(EntityID id)
    {
        const auto it = FindEntityIt(m_entities, id);
        if (it == m_entities.end())
            return;

        it->active = false;

        // Remove components for that entity (keeps stores coherent)
        m_sprites.Remove(id);
        m_collisions.Remove(id);
        m_names.Remove(id);

        RebuildNameLookup();
    }

    void Scene2D::Clear()
    {
        m_entities.clear();

        m_sprites.Clear();
        m_collisions.Clear();
        m_names.Clear();

        m_circlePool.clear();
        m_aabbPool.clear();
        m_nameLookup.clear();

        m_nextID = 1;
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

    Entity2D* Scene2D::FindByName(const std::string& name)
    {
        if (name.empty())
            return nullptr;

        const auto it = m_nameLookup.find(name);
        if (it == m_nameLookup.end())
            return nullptr;

        Entity2D* entity = FindEntity(it->second);
        return (entity && entity->active) ? entity : nullptr;
    }

    const Entity2D* Scene2D::FindByName(const std::string& name) const
    {
        if (name.empty())
            return nullptr;

        const auto it = m_nameLookup.find(name);
        if (it == m_nameLookup.end())
            return nullptr;

        const Entity2D* entity = FindEntity(it->second);
        return (entity && entity->active) ? entity : nullptr;
    }

    // ------------------------------------------------------------------------

    SpriteRenderer2D& Scene2D::AddSprite(EntityID id)
    {
        return m_sprites.Add(id);
    }

    NameComponent& Scene2D::AddName(EntityID id, const std::string& name)
    {
        NameComponent& n = m_names.Add(id);
        n.name = name;

        if (!name.empty())
            m_nameLookup[name] = id;

        return n;
    }

    NameComponent* Scene2D::TryGetName(EntityID id)
    {
        return m_names.TryGet(id);
    }

    const NameComponent* Scene2D::TryGetName(EntityID id) const
    {
        return m_names.TryGet(id);
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

    // ------------------------------------------------------------------------

    void Scene2D::Update(float dt)
    {
        KBK_UNUSED(dt);
    }

    void Scene2D::RebuildNameLookup()
    {
        m_nameLookup.clear();
        m_nameLookup.reserve(m_entities.size());

        for (const auto& e : m_entities) {
            if (!e.active)
                continue;

            const NameComponent* n = m_names.TryGet(e.id);
            if (!n || n->name.empty())
                continue;

            m_nameLookup[n->name] = e.id;
        }
    }

    void Scene2D::Render(SpriteBatch2D& batch) const
    {
        for (const auto& e : m_entities) {
            if (!e.active)
                continue;

            const auto* spr = m_sprites.TryGet(e.id);
            if (!spr || !spr->texture || !spr->texture->IsValid())
                continue;

            const RectF& local = spr->dst;
            const Transform2D& t = e.transform;

            const float w = local.w * t.scale.x;
            const float h = local.h * t.scale.y;

            RectF dst{};
            dst.w = w;
            dst.h = h;
            dst.x = t.position.x - (w * 0.5f);
            dst.y = t.position.y - (h * 0.5f);

            batch.Push(
                *spr->texture,
                dst,
                spr->src,
                spr->color,
                t.rotation,
                spr->layer);
        }
    }

    // ------------------------------------------------------------------------

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

        // entities array validation
        auto itEntities = root.find("entities");
        if (itEntities == root.end() || !itEntities->is_array()) {
            KbkWarn(kLogChannel, "LoadFromFile: no 'entities' array in '%s'", path);
            ResolveAssets(assets);
            return true; // scene empty is valid
        }

        const auto& entitiesJson = *itEntities;
        m_entities.reserve(entitiesJson.size());

        for (const auto& eJson : entitiesJson)
        {
            if (!eJson.is_object())
                continue;

            EntityID id = eJson.value("id", 0u);
            Entity2D& e = (id != 0) ? CreateEntityWithID(id) : CreateEntity();

            e.active = eJson.value("active", true);

            // name
            if (auto itName = eJson.find("name"); itName != eJson.end() && itName->is_string()) {
                AddName(e.id, itName->get<std::string>());
            }

            // transform
            if (auto itT = eJson.find("transform"); itT != eJson.end() && itT->is_object()) {
                const auto& t = *itT;

                if (auto it = t.find("pos"); it != t.end())
                    e.transform.position = ReadVec2(*it, 0.0f, 0.0f);

                if (auto it = t.find("rot"); it != t.end() && it->is_number())
                    e.transform.rotation = it->get<float>();

                if (auto it = t.find("scale"); it != t.end())
                    e.transform.scale = ReadVec2(*it, 1.0f, 1.0f);
            }

            // sprite
            if (auto itS = eJson.find("sprite"); itS != eJson.end() && itS->is_object()) {
                auto& spr = AddSprite(e.id);
                const auto& s = *itS;

                if (auto itTex = s.find("texture"); itTex != s.end() && itTex->is_object()) {
                    const auto& tex = *itTex;

                    if (auto it = tex.find("id"); it != tex.end() && it->is_string())
                        spr.textureId = it->get<std::string>();

                    if (auto it = tex.find("path"); it != tex.end() && it->is_string())
                        spr.texturePath = it->get<std::string>();

                    if (auto it = tex.find("sRGB"); it != tex.end() && it->is_boolean())
                        spr.textureSRGB = it->get<bool>();
                }

                if (auto it = s.find("dst"); it != s.end())
                    spr.dst = ReadRectF(*it, spr.dst);

                if (auto it = s.find("src"); it != s.end())
                    spr.src = ReadRectF(*it, spr.src);

                if (auto it = s.find("color"); it != s.end())
                    spr.color = ReadColor4(*it, spr.color);

                if (auto it = s.find("layer"); it != s.end() && it->is_number_integer())
                    spr.layer = it->get<int>();
            }

            // collision
            if (auto itC = eJson.find("collision"); itC != eJson.end() && itC->is_object()) {
                const auto& c = *itC;

                const std::string type = c.value("type", "");
                const bool active = c.value("active", true);

                if (type == "circle") {
                    const float radius = c.value("radius", 0.0f);
                    AddCircleCollider(e.id, radius, active);
                }
                else if (type == "aabb") {
                    AddAABBCollider(
                        e.id,
                        c.value("halfW", 0.0f),
                        c.value("halfH", 0.0f),
                        active);
                }
            }
        }

        ResolveAssets(assets);

        KbkLog(kLogChannel, "Loaded scene '%s' (%zu entities)", path, m_entities.size());
        return true;
    }

    void Scene2D::ResolveAssets(AssetManager& assets)
    {
        m_sprites.ForEach([&](EntityID /*id*/, SpriteRenderer2D& spr) {

            if (spr.texture && spr.texture->IsValid())
                return;

            if (spr.texturePath.empty())
                return;

            // Fallback: if no id provided, use path as cache key
            const std::string& key = spr.textureId.empty() ? spr.texturePath : spr.textureId;

            spr.texture = assets.LoadTexture(
                key,
                spr.texturePath,
                spr.textureSRGB);
            });
    }

} // namespace KibakoEngine

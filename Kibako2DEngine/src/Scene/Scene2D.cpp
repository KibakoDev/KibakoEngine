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

        static std::string ReadAllText(const char* path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
                return {};
            std::ostringstream ss;
            ss << file.rdbuf();
            return ss.str();
        }

        static DirectX::XMFLOAT2 ReadVec2(const nlohmann::json& arr, float dx, float dy)
        {
            if (!arr.is_array() || arr.size() < 2)
                return { dx, dy };
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
            return {
                arr[0].get<float>(),
                arr[1].get<float>(),
                arr[2].get<float>(),
                arr[3].get<float>()
            };
        }
    }

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
        m_sprites.Remove(id);
        m_collisions.Remove(id);
        m_names.Remove(id);
    }

    void Scene2D::Clear()
    {
        m_entities.clear();
        m_sprites.Clear();
        m_collisions.Clear();
        m_names.Clear();
        m_circlePool.clear();
        m_aabbPool.clear();
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
        for (auto& e : m_entities) {
            if (!e.active) continue;
            if (auto* n = m_names.TryGet(e.id); n && n->name == name)
                return &e;
        }
        return nullptr;
    }

    const Entity2D* Scene2D::FindByName(const std::string& name) const
    {
        for (const auto& e : m_entities) {
            if (!e.active) continue;
            if (auto* n = m_names.TryGet(e.id); n && n->name == name)
                return &e;
        }
        return nullptr;
    }

    // ------------------------------------------------------------------------

    SpriteRenderer2D& Scene2D::AddSprite(EntityID id)
    {
        return m_sprites.Add(id);
    }

    NameComponent& Scene2D::AddName(EntityID id, const std::string& name)
    {
        auto& n = m_names.Add(id);
        n.name = name;
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

    void Scene2D::Render(SpriteBatch2D& batch) const
    {
        for (const auto& e : m_entities) {
            if (!e.active) continue;

            const auto* spr = m_sprites.TryGet(e.id);
            if (!spr || !spr->texture || !spr->texture->IsValid())
                continue;

            const RectF& local = spr->dst;
            const Transform2D& t = e.transform;

            const float w = local.w * t.scale.x;
            const float h = local.h * t.scale.y;

            RectF dst;
            dst.w = w;
            dst.h = h;
            dst.x = t.position.x - w * 0.5f;
            dst.y = t.position.y - h * 0.5f;

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
        const std::string text = ReadAllText(path);
        if (text.empty())
            return false;

        nlohmann::json root = nlohmann::json::parse(text);
        Clear();

        for (const auto& eJson : root["entities"])
        {
            EntityID id = eJson.value("id", 0u);
            Entity2D& e = (id != 0) ? CreateEntityWithID(id) : CreateEntity();

            e.active = eJson.value("active", true);

            if (eJson.contains("name"))
                AddName(e.id, eJson["name"].get<std::string>());

            if (auto t = eJson.find("transform"); t != eJson.end()) {
                if (t->contains("pos"))
                    e.transform.position = ReadVec2((*t)["pos"], 0, 0);
                if (t->contains("rot"))
                    e.transform.rotation = (*t)["rot"].get<float>();
                if (t->contains("scale"))
                    e.transform.scale = ReadVec2((*t)["scale"], 1, 1);
            }

            if (auto s = eJson.find("sprite"); s != eJson.end()) {
                auto& spr = AddSprite(e.id);
                if (auto tex = s->find("texture"); tex != s->end()) {
                    spr.textureId = tex->value("id", "");
                    spr.texturePath = tex->value("path", "");
                    spr.textureSRGB = tex->value("sRGB", true);
                }
                if (s->contains("dst"))   spr.dst = ReadRectF((*s)["dst"], spr.dst);
                if (s->contains("src"))   spr.src = ReadRectF((*s)["src"], spr.src);
                if (s->contains("color")) spr.color = ReadColor4((*s)["color"], spr.color);
                if (s->contains("layer")) spr.layer = (*s)["layer"].get<int>();
            }

            if (auto c = eJson.find("collision"); c != eJson.end()) {
                const std::string type = c->value("type", "");
                const bool active = c->value("active", true);

                if (type == "circle")
                    AddCircleCollider(e.id, c->value("radius", 0.0f), active);
                else if (type == "aabb")
                    AddAABBCollider(
                        e.id,
                        c->value("halfW", 0.0f),
                        c->value("halfH", 0.0f),
                        active);
            }
        }

        ResolveAssets(assets);
        return true;
    }

    void Scene2D::ResolveAssets(AssetManager& assets)
    {
        m_sprites.ForEach([&](EntityID, SpriteRenderer2D& spr) {
            if (!spr.texture && !spr.texturePath.empty()) {
                spr.texture = assets.LoadTexture(
                    spr.textureId,
                    spr.texturePath,
                    spr.textureSRGB);
            }
            });
    }

} // namespace KibakoEngine
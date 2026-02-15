#pragma once

#include <vector>
#include <unordered_map>
#include <utility>
#include <cstddef>
#include <cstdint>

namespace KibakoEngine {

    using EntityID = std::uint32_t;

    // Sparse-set style store:
    // - Dense arrays for fast iteration
    // - Sparse map for O(1) lookup/removal
    template<typename T>
    class ComponentStore
    {
    public:
        void Reserve(std::size_t count)
        {
            m_dense.reserve(count);
            m_denseEntities.reserve(count);
            m_sparse.reserve(count);
        }

        bool Has(EntityID id) const
        {
            return m_sparse.find(id) != m_sparse.end();
        }

        // Adds component if missing, returns existing otherwise.
        T& Add(EntityID id, const T& value = T{})
        {
            auto it = m_sparse.find(id);
            if (it != m_sparse.end())
                return m_dense[it->second];

            const std::size_t index = m_dense.size();
            m_dense.push_back(value);
            m_denseEntities.push_back(id);
            m_sparse.emplace(id, index);
            return m_dense.back();
        }

        T* TryGet(EntityID id)
        {
            auto it = m_sparse.find(id);
            if (it == m_sparse.end())
                return nullptr;
            return &m_dense[it->second];
        }

        const T* TryGet(EntityID id) const
        {
            auto it = m_sparse.find(id);
            if (it == m_sparse.end())
                return nullptr;
            return &m_dense[it->second];
        }

        void Remove(EntityID id)
        {
            auto it = m_sparse.find(id);
            if (it == m_sparse.end())
                return;

            const std::size_t index = it->second;
            const std::size_t last = m_dense.size() - 1;

            if (index != last)
            {
                // Move last into removed slot (swap-remove)
                m_dense[index] = std::move(m_dense[last]);
                m_denseEntities[index] = m_denseEntities[last];

                // Fix sparse index of moved entity
                const EntityID movedId = m_denseEntities[index];
                m_sparse[movedId] = index;
            }

            m_dense.pop_back();
            m_denseEntities.pop_back();
            m_sparse.erase(it);
        }

        void Clear()
        {
            m_dense.clear();
            m_denseEntities.clear();
            m_sparse.clear();
        }

        std::size_t Size() const { return m_dense.size(); }

        template<typename Fn>
        void ForEach(Fn&& fn)
        {
            for (std::size_t i = 0; i < m_dense.size(); ++i)
                fn(m_denseEntities[i], m_dense[i]);
        }

        template<typename Fn>
        void ForEach(Fn&& fn) const
        {
            for (std::size_t i = 0; i < m_dense.size(); ++i)
                fn(m_denseEntities[i], m_dense[i]);
        }

    private:
        std::vector<T> m_dense;
        std::vector<EntityID> m_denseEntities;
        std::unordered_map<EntityID, std::size_t> m_sparse;
    };

} // namespace KibakoEngine

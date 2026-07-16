#pragma once
#include "Defines.h"

using EntityID = uint32_t;
using TypeID = uint32_t;

inline std::atomic<TypeID> g_TypeCounter{};
inline std::atomic<EntityID> g_IDCounter{};

inline constexpr EntityID InvalidEntityID = std::numeric_limits<EntityID>::max();
inline constexpr TypeID InvalidTypeID = std::numeric_limits<TypeID>::max();

template<typename T>
struct EntityKey
{
    const EntityID entID = g_IDCounter++;
    inline static const TypeID typID = g_TypeCounter++;
};

struct ObserverKey
{
    ObserverKey() {}

    template<typename T>
    explicit ObserverKey(EntityKey<T> key) {
        this->entID = key.entID;
        this->typID = key.typID;
    }

    EntityID entID = InvalidEntityID;
    TypeID typID = InvalidTypeID;
};

template<typename T>
struct EntityMap
{
    std::shared_ptr<T> Add()
    {
        auto val = std::make_shared<T>(T{});
        map.emplace(val->m_id.entID, val);
        return val;
    }
    std::shared_ptr<T> Add(std::shared_ptr<T> val)
    {
        auto [iter, inserted] = map.emplace(val->m_id.entID, val);
        return iter->second;
    }
    std::shared_ptr<T> Get(ObserverKey key) const
    {
        ASSERT(map.contains(key.entID));

        return map[key.entID];
    }
    std::shared_ptr<T> Get(EntityID id) const
    {
        ASSERT(map.contains(id));

        return map[id];
    }
    std::shared_ptr<const T> CGet(ObserverKey key) const
    {
        ASSERT(map.contains(key.entID));

        return map.at(key.entID);
    }
    std::shared_ptr<const T> CGet(EntityID id) const
    {
        ASSERT(map.contains(id));

        return map.at(id);
    }
    auto Begin()
    {
        return map.begin();
    }
    auto CBegin() {
        return map.cbegin();
    }
    auto End()
    {
        return map.end()();
    }
    auto CEnd() {
        return map.cend();
    }
    void ForEach(std::function<LoopCondition(EntityID, std::shared_ptr<T>)> eval)
    {
        for (auto& [key, val] : map)
        {
            LoopCondition cnd{ eval(key, val) };

            if (cnd.condition.HasLeastOne(
                ELoopConditionFlag::BREAK |
                ELoopConditionFlag::SUCCESS |
                ELoopConditionFlag::UNDEFINED
            ))
            {
                break;
            }
        }
    }
    void CForEach(std::function<LoopCondition(EntityID, std::shared_ptr<const T>)> eval) const
    {
        for (auto& [key, val] : map)
        {
            LoopCondition cnd{ eval(key, val) };

            if (cnd.condition.HasLeastOne(
                ELoopConditionFlag::BREAK |
                ELoopConditionFlag::SUCCESS |
                ELoopConditionFlag::UNDEFINED
            ))
            {
                break;
            }
        }
    }
    bool Contains(ObserverKey key) const
    {
        return map.contains(key.entID);
    }
    bool Contains(EntityID id) const
    {
        return map.contains(id);
    }
    void Reserve(size_t size)
    {
        map.reserve(size);
    }
    bool Erase(ObserverKey key)
    {
        auto iter = map.find(key.entID);
        if (iter != map.end())
        {
            map.erase(iter);
            return true;
        }
        return false;
    }
    void Clear()
    {
        map.clear();
    }
    size_t Size() const
    {
        return map.size();
    }
    [[nodiscard]] bool Empty() const
    {
        map.empty();
    }

private:
    std::unordered_map<EntityID, std::shared_ptr<T>> map;
};

template<typename TLHS, typename TRHS>
constexpr bool operator==(EntityKey<TLHS>& lhs, EntityKey<TRHS>& rhs)
{
    if constexpr (not std::same_as<TLHS, TRHS>) return false;

    return lhs.entID == rhs.entID;
}

template<typename TLHS>
constexpr bool operator==(EntityKey<TLHS>& lhs, ObserverKey& rhs)
{
    return lhs.entID == rhs.entID and lhs.typID == rhs.typID;
}
template<typename TRHS>
constexpr bool operator==(ObserverKey& lhs, EntityKey<TRHS>& rhs)
{
    return lhs.entID == rhs.entID and lhs.typID == rhs.typID;
}

constexpr bool operator==(ObserverKey& lhs, ObserverKey& rhs)
{
    return lhs.entID == rhs.entID and lhs.typID == rhs.typID;
}

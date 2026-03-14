#pragma once

#include "RendererTypes.h"

class Blackboard
{
public:
    Blackboard() = default;
    ~Blackboard() = default;

    Blackboard(const Blackboard&) = delete;
    Blackboard& operator=(const Blackboard&) = delete;
    Blackboard(Blackboard&&) = default;
    Blackboard& operator=(Blackboard&&) = default;

    template<typename T>
    void Set(NSRenderer::BlackboardKey key, const T& value)
    {
        m_entries[key.name] = value;
    }

    template<typename T>
    void Set(NSRenderer::BlackboardKey key, T&& value)
    {
        m_entries[key.name] = std::move(value);
    }

    template<typename T>
    T* GetMut(NSRenderer::BlackboardKey key)
    {
        auto it = m_entries.find(key.name);

        if (it == m_entries.end()) return nullptr;

        return std::any_cast<T>(&it->second);
    }

    template<typename T>
    const T* GetConst(NSRenderer::BlackboardKey key) const
    {
        auto it = m_entries.find(key.name);

        if (it == m_entries.end()) return nullptr;

        return std::any_cast<T>(&it->second);
    }

    template<typename T>
    std::optional<T> GetOpt(NSRenderer::BlackboardKey key) const
    {
        const T* ptr = Get<T>(key);
        return ptr ? std::optional<T>(*ptr) : std::nullopt;
    }

    bool Contains(NSRenderer::BlackboardKey key)
    {
        return m_entries.contains(key.name);
    }

    void Erase(NSRenderer::BlackboardKey key)
    {
        m_entries.erase(key.name);
    }

    void Reset()
    {
        m_entries.clear();
    }

    size_t Size() const { return m_entries.size(); }
    [[nodiscard]] bool Empty() const { return m_entries.empty(); }

private:
    std::unordered_map<std::string, std::any> m_entries;
};


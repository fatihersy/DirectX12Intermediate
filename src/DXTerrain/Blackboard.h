#pragma once

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
    void Set(NSRenderer::BlackboardKey key, T&& value)
    {
        m_entries[key.name] = std::forward<T>(value);
    }

    template<typename T>
    T* GetMut(NSRenderer::BlackboardKey key)
    {
        using RawT = std::remove_cv_t<T>;

        auto it = m_entries.find(key.name);

        if (it == m_entries.end()) return nullptr;

        return std::any_cast<RawT>(&it->second);
    }

    template<typename T>
    const T* GetConst(NSRenderer::BlackboardKey key) const
    {
        using RawT = std::remove_cv_t<T>;

        auto it = m_entries.find(key.name);

        if (it == m_entries.end()) return nullptr;

        return std::any_cast<RawT>(&it->second);
    }

    template<typename T>
    std::optional<std::reference_wrapper<T>> GetOpt(NSRenderer::BlackboardKey key)
    {
        T* ptr = GetMut<T>(key);

        return ptr ? std::optional<std::reference_wrapper<T>>{*ptr} : std::nullopt;
    }

    template<typename T>
    std::optional<std::reference_wrapper<const T>> GetOpt(NSRenderer::BlackboardKey key) const
    {
        const T* ptr = GetConst<T>(key);

        return ptr ? std::optional<std::reference_wrapper<const T>>{*ptr} : std::nullopt;
    }

private:
    std::unordered_map<std::string, std::any> m_entries;
};

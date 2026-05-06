#pragma once

#include <string>

class FString {
public:
    // No-args overload: return the string directly without snprintf
    static std::string format(const std::string& str)
    {
        return str;
    }

    template<typename... Args>
    static std::string format(const std::string& format, Args&&... args)
    {
        auto to_str = []<typename T>(const T & v) -> decltype(auto)
        {
            using U = std::remove_cvref_t<T>;

            if constexpr (std::is_same_v<U, std::string>) return v.c_str();
            else if constexpr (std::is_same_v<U, std::string_view>) return v.data();
            else return v;
        };

        int size = std::snprintf(nullptr, 0, format.c_str(), to_str(args)...);
        if (size < 0) return {};
        std::vector<char> buf(size + 1);
        std::snprintf(buf.data(), buf.size(), format.c_str(), to_str(args)...);
        return std::string(buf.data(), size);
    }

    // No-args overload: return the string directly without swprintf
    static std::wstring wformat(const std::wstring& str)
    {
        return str;
    }

    template<typename... Args>
    static std::wstring wformat(const std::wstring& format, Args&&... args)
    {
        auto to_wstr = []<typename T>(const T & v) -> decltype(auto)
        {
            using U = std::remove_cvref_t<T>;

            if constexpr (std::is_same_v<U, std::wstring>) return v.c_str();
            else if constexpr (std::is_same_v<U, std::wstring_view>) return v.data();
            else return v;
        };

        int size = std::swprintf(nullptr, 0, format.c_str(), to_wstr(args)...);
        if (size < 0) return {};
        std::vector<wchar_t> buf(size + 1);
        std::swprintf(buf.data(), buf.size(), format.c_str(), to_wstr(args)...);
        return std::wstring(buf.data(), size);
    }
};

inline bool Float3Equals(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, const float epsilon = 1e-6f) noexcept
{
    using namespace DirectX;

    const XMVECTOR va = XMLoadFloat3(&a);
    const XMVECTOR vb = XMLoadFloat3(&b);

    const XMVECTOR diff = XMVectorAbs(XMVectorSubtract(va, vb));

    const XMVECTOR epsilonVec = XMVectorReplicate(epsilon);

    return XMVector3Less(diff, epsilonVec);
};

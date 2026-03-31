#pragma once

namespace NSTool
{
    template<typename... Args>
    static std::string format(const std::string& format, Args&&... args)
    {
        auto to_str = []<typename T>(const T & v) -> decltype(auto)
        {
            if constexpr (std::is_same_v<T, std::string>) return v.c_str();
            else return v;
        };

        int size = std::snprintf(nullptr, 0, format.c_str(), to_str(args)...);
        if (size < 0) return {};
        std::vector<char> buf(size + 1);
        std::snprintf(buf.data(), buf.size(), format.c_str(), to_str(args)...);
        return std::string(buf.data(), size);
    }
    template<typename... Args>
    static std::wstring wformat(const std::wstring& format, Args&&... args)
    {
        auto to_wstr = []<typename T>(const T & v) -> decltype(auto)
        {
            if constexpr (std::is_same_v<T, std::wstring>) return v.c_str();
            else return v;
        };

        int size = std::swprintf(nullptr, 0, format.c_str(), to_wstr(args)...);
        if (size < 0) return {};
        std::vector<wchar_t> buf(size + 1);
        std::swprintf(buf.data(), buf.size(), format.c_str(), to_wstr(args)...);
        return std::wstring(buf.data(), size);
    }

    bool Float3Equals(const DirectX::XMFLOAT3& lhs, const DirectX::XMFLOAT3& rhs, const float epsilon = DirectX::g_XMEpsilon.f[0]) noexcept
    {
        using namespace DirectX;

        const XMVECTOR vLHS = XMLoadFloat3(&lhs);
        const XMVECTOR vRHS = XMLoadFloat3(&rhs);

        const XMVECTOR diff = XMVectorAbs(XMVectorSubtract(vLHS, vRHS));

        const XMVECTOR vEps = XMVectorReplicate(epsilon);

        return XMVector3Less(diff, vEps);
    }
};

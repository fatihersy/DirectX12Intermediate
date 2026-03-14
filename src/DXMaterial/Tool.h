#pragma once

#include <string>

class FString {
public:
    template<typename... Args>
    static std::string format(const std::string& format, Args&&... args)
    {
        int size = std::snprintf(nullptr, 0, format.c_str(), to_str(args)...);
        if (size < 0) return {};
        std::vector<char> buf(size + 1);
        std::snprintf(buf.data(), buf.size(), format.c_str(), to_str(args)...);
        return std::string(buf.data(), size);
    }

    template<typename... Args>
    static std::wstring wformat(const std::wstring& format, Args&&... args)
    {
        int size = std::swprintf(nullptr, 0, format.c_str(), to_wstr(args)...);
        if (size < 0) return {};
        std::vector<wchar_t> buf(size + 1);
        std::swprintf(buf.data(), buf.size(), format.c_str(), to_wstr(args)...);
        return std::wstring(buf.data(), size);
    }

private:
    static const char* to_str(const std::string& t)
    {
        return t.c_str();
    }
    static const wchar_t* to_wstr(const std::wstring& t)
    {
        return t.c_str();
    }

    // universal reference here would be always selected, including std::string
    template<typename T>
    static T to_str(const T& t)
    {
        return t;
    }
    template<typename T>
    static T to_wstr(const T& t)
    {
        return t;
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

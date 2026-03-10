#pragma once

#include <string>

class FString {
public:
    template<typename... Args>
    static inline std::string format(const std::string& format, Args&&... args)
    {
        int size = std::snprintf(nullptr, 0, format.c_str(), to_str(args)...);
        if (size < 0) return {};
        std::unique_ptr<char[]> buf(new char[size + 1]);
        std::snprintf(buf.get(), size + 1, format.c_str(), to_str(args)...);
        return std::string(buf.get(), buf.get() + size);
    }

    template<typename... Args>
    static inline std::wstring wformat(const std::string& format, Args&&... args)
    {
        int size = std::snprintf(nullptr, 0, format.c_str(), to_str(args)...);
        if (size < 0) return {};
        std::unique_ptr<char[]> buf(new char[size + 1]);
        std::snprintf(buf.get(), size + 1, format.c_str(), to_str(args)...);
        return std::wstring(buf.get(), buf.get() + size - 1);
    }

private:
    static const char* to_str(const std::string& t)
    {
        return t.c_str();
    }

    // universal reference here would be always selected, including std::string
    template<typename T>
    static T to_str(const T& t)
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

#pragma once

namespace NSTool
{
    inline float Saturate(float v)
    {
        return std::clamp(v, 0.0f, 1.0f);
    }

    inline float Smoothstep(float t)
    {
        return t * t * (3.0f - 2.0f * t);
    }

    inline float Lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    inline uint32_t Hash(uint32_t x, uint32_t z, uint32_t seed)
    {
        uint32_t h = seed;
        h ^= x * 374761393u;
        h ^= z * 668265263u;
        h = (h ^ (h >> 13u)) * 1274126177u;
        return h ^ (h >> 16u);
    }

    inline float Hash01(int x, int z, uint32_t seed)
    {
        uint32_t h = Hash(
            static_cast<uint32_t>(x),
            static_cast<uint32_t>(z),
            seed
        );

        return float(h & 0x00FFFFFFu) / float(0x01000000u);
    }

    inline float ValueNoise(float x, float z, uint32_t seed)
    {
        int x0 = static_cast<int>(std::floor(x));
        int z0 = static_cast<int>(std::floor(z));
        int x1 = x0 + 1;
        int z1 = z0 + 1;

        float tx = x - static_cast<float>(x0);
        float tz = z - static_cast<float>(z0);

        tx = Smoothstep(tx);
        tz = Smoothstep(tz);

        float v00 = Hash01(x0, z0, seed);
        float v10 = Hash01(x1, z0, seed);
        float v01 = Hash01(x0, z1, seed);
        float v11 = Hash01(x1, z1, seed);

        float vx0 = Lerp(v00, v10, tx);
        float vx1 = Lerp(v01, v11, tx);

        return Lerp(vx0, vx1, tz);
    }

    inline float FBm(float x, float z, uint32_t seed)
    {
        float value = 0.0f;
        float amplitude = 0.5f;
        float frequency = 1.0f;

        for (int octave = 0; octave < 8; ++octave)
        {
            value += amplitude * ValueNoise(x * frequency, z * frequency, seed + octave * 1013u);
            frequency *= 2.0f;
            amplitude *= 0.5f;
        }

        return Saturate(value);
    }

    template<typename... Args>
    static std::string format(const std::string_view& format, Args&&... args)
    {
        auto to_str = []<typename T>(const T & v) -> decltype(auto)
        {
            if constexpr (std::is_same_v<T, std::string>) return v.c_str();
            else return v;
        };

        int size = std::snprintf(nullptr, 0, format.data(), to_str(args)...);
        if (size < 0) return {};
        std::vector<char> buf(static_cast<size_t>(size + 1));
        std::snprintf(buf.data(), buf.size(), format.data(), to_str(args)...);
        return std::string(buf.data(), size);
    }
    template<typename... Args>
    static std::wstring wformat(const std::wstring_view& format, Args&&... args)
    {
        auto to_wstr = []<typename T>(const T & v) -> decltype(auto)
        {
            if constexpr (std::is_same_v<T, std::wstring>) return v.c_str();
            else return v;
        };

        int size = std::swprintf(nullptr, 0, format.data(), to_wstr(args)...);
        if (size < 0) return {};
        std::vector<wchar_t> buf(static_cast<size_t>(size + 1));
        std::swprintf(buf.data(), buf.size(), format.data(), to_wstr(args)...);
        return std::wstring(buf.data(), size);
    }

    static bool Float3Equals(const DirectX::XMFLOAT3& lhs, const DirectX::XMFLOAT3& rhs, const float epsilon = DirectX::g_XMEpsilon.f[0]) noexcept
    {
        using namespace DirectX;

        const XMVECTOR vLHS = XMLoadFloat3(&lhs);
        const XMVECTOR vRHS = XMLoadFloat3(&rhs);

        const XMVECTOR diff = XMVectorAbs(XMVectorSubtract(vLHS, vRHS));

        const XMVECTOR vEps = XMVectorReplicate(epsilon);

        return XMVector3Less(diff, vEps);
    }
};

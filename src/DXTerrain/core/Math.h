#pragma once

namespace NSMath
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
    static bool fLessThan(float lhs, float rhs, float epsilon = DirectX::g_XMEpsilon.f[0]) noexcept
    {
        return lhs - rhs < -epsilon;
    }
    static bool fLessEqual(float lhs, float rhs, float epsilon = DirectX::g_XMEpsilon.f[0]) noexcept
    {
        return lhs - rhs <= epsilon;
    }
    static bool fGreaterThan(float lhs, float rhs, float epsilon = DirectX::g_XMEpsilon.f[0]) noexcept
    {
        return lhs - rhs > epsilon;
    }
    static bool fGreaterEqual(float lhs, float rhs, float epsilon = DirectX::g_XMEpsilon.f[0]) noexcept
    {
        return lhs - rhs >= -epsilon;
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
    struct SBoundSphere {
        DirectX::XMFLOAT3 position{};
        float radius{};
        UINT sliceCount{};
        UINT stackCount{};
    };
    struct SBoundAABB {
        DirectX::XMFLOAT3 min{};
        DirectX::XMFLOAT3 max{};
    };
    struct SRectU32
    {
        uint32_t x{};
        uint32_t y{};
        uint32_t width{};
        uint32_t height{};
    };
    struct SRectF32
    {
        float x{};
        float y{};
        float width{};
        float height{};
    };
    struct SFrustum
    {
        SFrustum() {};
        SFrustum(DirectX::XMMATRIX viewProj);

        bool TestSphere(SBoundSphere& bound) const
        {
            using namespace DirectX;

            for (size_t i{}; i < 6; i++)
            {
                const float dist = XMVectorGetX(XMPlaneDotCoord(Planes[i], XMLoadFloat3(&bound.position)));

                if (dist < -bound.radius) return false;
            }

            return true;
        }
        bool TestAABB(const SBoundAABB& bound) const
        {
            using namespace DirectX;

            for (size_t i{}; i < 6; i++)
            {
                XMVECTOR pVertex = XMVectorSelect(
                    XMLoadFloat3(&bound.min),
                    XMLoadFloat3(&bound.max),
                    XMVectorGreaterOrEqual(Planes[i],
                    XMVectorZero())
                );

                if (XMVectorGetX(XMPlaneDotCoord(Planes[i], pVertex)) < 0.f) return false;
            }

            return true;
        }

        DirectX::XMVECTOR Planes[6]{};
    };
}

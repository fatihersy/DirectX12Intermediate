#pragma once
#include "Defines.h"

#include "EntityTypes.h"

namespace NSMath
{
    inline float Saturate(float v)
    {
        return std::clamp(v, 0.0f, 1.0f);
    }
    inline float Smoothstep(float t)
    {
        return t * t * (3.f - 2.f * t);
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

        return static_cast<float>(h & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
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
        float value{};
        float amplitude = .5f;
        float frequency = 1.f;

        for (int octave{}; octave < 8; ++octave)
        {
            value += amplitude * ValueNoise(x * frequency, z * frequency, seed + octave * 1013u);
            frequency *= 2.f;
            amplitude *= .5f;
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
    static bool Float3Equals(const DirectX::XMFLOAT3& lhs, const DirectX::XMFLOAT3& rhs, float epsilon = DirectX::g_XMEpsilon.f[0])
    {
        using namespace DirectX;

        const XMVECTOR vLHS = XMLoadFloat3(&lhs);
        const XMVECTOR vRHS = XMLoadFloat3(&rhs);
        const XMVECTOR vEps = XMVectorReplicate(epsilon);

        const XMVECTOR diff = XMVectorAbs(XMVectorSubtract(vLHS, vRHS));

        return XMVector3Less(diff, vEps);
    }
    struct SRectU32
    {
        uint32_t x{};
        uint32_t y{};
        uint32_t width{};
        uint32_t height{};
    };
    struct SRectI32
    {
        int32_t x{};
        int32_t y{};
        int32_t width{};
        int32_t height{};
    };
    struct SRectF32
    {
        float x{};
        float y{};
        float width{};
        float height{};
    };
    struct SBoundSphere
    {
        DirectX::XMFLOAT3 position{};
        float radius{};
        uint32_t sliceCount{};
        uint32_t stackCount{};
    };
    struct SBoundAABB
    {
        DirectX::XMFLOAT3 min{};
        DirectX::XMFLOAT3 max{};
    };

    using BoundingBox = std::variant<SBoundSphere, SBoundAABB>;

    struct ICullable
    {
        ObserverKey ICullable_Id;
        BoundingBox ICullable_Bound;
    };

    struct SFrustum
    {
        SFrustum() {};
        SFrustum(DirectX::XMMATRIX viewProj);

        DirectX::XMVECTOR planes[6]{};

        bool TestBounds(const BoundingBox& bb) const
        {
            using namespace DirectX;

            bool collides = true;

            std::visit(overloaded
            {
                [this, &collides](const SBoundSphere& sphere)
                {
                    for (DirectX::XMVECTOR plane : planes)
                    {
                        const float dist = XMVectorGetX(XMPlaneDotCoord(plane, XMLoadFloat3(&sphere.position)));

                        if (dist < -sphere.radius)
                        {
                            collides = false;
                            break;
                        }
                    }
                },
                [this, &collides](const SBoundAABB& aabb)
                {
                    for (DirectX::XMVECTOR plane : planes)
                    {
                        XMVECTOR pVertex = XMVectorSelect(
                            XMLoadFloat3(&aabb.min),
                            XMLoadFloat3(&aabb.max),
                            XMVectorGreaterOrEqual(plane, XMVectorZero())
                        );

                        if (XMVectorGetX(XMPlaneDotCoord(plane, pVertex)) < 0.f)
                        {
                            collides = false;
                            break;
                        }
                    }
                }
            }, bb);

            return collides;
        }
    };
}

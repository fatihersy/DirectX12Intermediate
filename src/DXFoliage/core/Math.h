#pragma once
#include "Defines.h"

#include "EntityTypes.h"

// Engine math types. These replace the DirectXMath (XMFLOAT3/XMVECTOR/...)
// types that used to be used directly: even though DirectXMath is largely
// portable, it's a Windows-SDK-shipped, API-flavoured dependency, and the
// neutral layers of this engine shouldn't be tied to it.
//
// Conventions (kept identical to what DirectXMath used, so shader-facing
// data and existing semantics are unchanged):
//   - Storage is ROW-MAJOR: Float4x4::m[row][col], and _rc names row r,
//     col c (1-based), matching XMFLOAT4X4.
//   - Row-vector convention: transforms compose as v * M, so a combined
//     matrix is World * View * Proj.
//   - Layout is plain floats with no padding, so these are drop-in
//     replacements inside constant-buffer structs (the 16-byte-alignment
//     static_asserts here still hold).
//
// Scalar (non-SIMD) implementation on purpose: it's simple, portable and
// correct. If profiling ever shows math is hot, the internals can move to
// SIMD without changing this interface.
namespace NSMath
{
    inline constexpr float kEpsilon = 1.192092896e-7f; // FLT_EPSILON, same value DirectX::g_XMEpsilon used
    inline constexpr float kPi = 3.14159265358979323846f;

    // --- Vectors -----------------------------------------------------

    struct Float2
    {
        float x{};
        float y{};

        constexpr Float2() = default;
        constexpr Float2(float inX, float inY) : x(inX), y(inY) {}
    };

    struct Float3
    {
        float x{};
        float y{};
        float z{};

        constexpr Float3() = default;
        constexpr Float3(float inX, float inY, float inZ) : x(inX), y(inY), z(inZ) {}
    };

    struct Float4
    {
        float x{};
        float y{};
        float z{};
        float w{};

        constexpr Float4() = default;
        constexpr Float4(float inX, float inY, float inZ, float inW) : x(inX), y(inY), z(inZ), w(inW) {}
        constexpr Float4(const Float3& v, float inW) : x(v.x), y(v.y), z(v.z), w(inW) {}
    };

    // --- Vector operators --------------------------------------------

    constexpr Float3 operator+(const Float3& a, const Float3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    constexpr Float3 operator-(const Float3& a, const Float3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    constexpr Float3 operator*(const Float3& a, float s)         { return { a.x * s, a.y * s, a.z * s }; }
    constexpr Float3 operator*(float s, const Float3& a)         { return a * s; }
    constexpr Float3 operator-(const Float3& a)                  { return { -a.x, -a.y, -a.z }; }

    constexpr Float4 operator+(const Float4& a, const Float4& b) { return { a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w }; }
    constexpr Float4 operator-(const Float4& a, const Float4& b) { return { a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w }; }
    constexpr Float4 operator*(const Float4& a, float s)         { return { a.x * s, a.y * s, a.z * s, a.w * s }; }

    constexpr Float2 operator+(const Float2& a, const Float2& b) { return { a.x + b.x, a.y + b.y }; }
    constexpr Float2 operator-(const Float2& a, const Float2& b) { return { a.x - b.x, a.y - b.y }; }
    constexpr Float2 operator*(const Float2& a, float s)         { return { a.x * s, a.y * s }; }

    // --- Vector functions --------------------------------------------

    constexpr float Dot(const Float3& a, const Float3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    constexpr float Dot(const Float4& a, const Float4& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

    constexpr Float3 Cross(const Float3& a, const Float3& b)
    {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    inline float LengthSq(const Float3& v) { return Dot(v, v); }
    inline float Length(const Float3& v) { return std::sqrt(Dot(v, v)); }

    inline Float3 Normalize(const Float3& v)
    {
        const float len = Length(v);
        return (len > kEpsilon) ? v * (1.f / len) : Float3{};
    }

    // --- Matrices ----------------------------------------------------

    // 4x4, row-major. The union mirrors XMFLOAT4X4 so both m[r][c] and
    // the _rc names work (same as the code this replaces).
    struct Float4x4
    {
        union
        {
            float m[4][4];
            struct
            {
                float _11, _12, _13, _14;
                float _21, _22, _23, _24;
                float _31, _32, _33, _34;
                float _41, _42, _43, _44;
            };
        };

        constexpr Float4x4()
            : _11(1.f), _12(0.f), _13(0.f), _14(0.f)
            , _21(0.f), _22(1.f), _23(0.f), _24(0.f)
            , _31(0.f), _32(0.f), _33(1.f), _34(0.f)
            , _41(0.f), _42(0.f), _43(0.f), _44(1.f)
        {}

        static constexpr Float4x4 Identity() { return Float4x4{}; }
    };

    // 3x4 (3 rows of 4) — used for normal matrices, where the 4th row is
    // implicit. Matches XMFLOAT3X4's layout.
    struct Float3x4
    {
        union
        {
            float m[3][4];
            struct
            {
                float _11, _12, _13, _14;
                float _21, _22, _23, _24;
                float _31, _32, _33, _34;
            };
        };

        constexpr Float3x4()
            : _11(1.f), _12(0.f), _13(0.f), _14(0.f)
            , _21(0.f), _22(1.f), _23(0.f), _24(0.f)
            , _31(0.f), _32(0.f), _33(1.f), _34(0.f)
        {}
    };

    inline Float4x4 Multiply(const Float4x4& a, const Float4x4& b)
    {
        Float4x4 result{};
        for (int row{}; row < 4; ++row)
        {
            for (int col{}; col < 4; ++col)
            {
                result.m[row][col] =
                    a.m[row][0] * b.m[0][col] +
                    a.m[row][1] * b.m[1][col] +
                    a.m[row][2] * b.m[2][col] +
                    a.m[row][3] * b.m[3][col];
            }
        }
        return result;
    }

    // --- Matrix builders ---------------------------------------------
    //
    // ROW-VECTOR, LEFT-HANDED, matching DirectXMath's XMMatrix*LH family
    // and therefore the conventions DXTerrain and DXMaterial already use.
    // Two consequences worth stating, because getting either wrong
    // produces geometry that is wrong in ways that look like other bugs:
    //
    //   Composition is left to right - Model * View * Projection - and a
    //   point transforms as v * M (see TransformPoint).
    //
    //   These are uploaded to shaders WITHOUT transposing. HLSL packs
    //   float4x4 column-major by default, so writing row-major data into
    //   one transposes it on arrival, and the shader then compensates by
    //   using mul(matrix, vector) rather than mul(vector, matrix). That
    //   cancellation is why the existing projects never call
    //   XMMatrixTranspose. Keep both halves or neither.
    //
    // Depth maps to 0..1, which is correct for D3D12 AND Vulkan (only
    // OpenGL wants -1..1), so one projection serves both backends. The
    // Y-flip Vulkan needs is handled by the negative-height viewport in
    // VulkanCommandList::SetViewport, not here.

    inline Float4x4 Translation(float x, float y, float z)
    {
        Float4x4 out{};
        out._41 = x; out._42 = y; out._43 = z;
        return out;
    }

    inline Float4x4 Scaling(float x, float y, float z)
    {
        Float4x4 out{};
        out._11 = x; out._22 = y; out._33 = z;
        return out;
    }

    inline Float4x4 RotationX(float radians)
    {
        const float c = std::cos(radians);
        const float s = std::sin(radians);

        Float4x4 out{};
        out._22 = c;  out._23 = s;
        out._32 = -s; out._33 = c;
        return out;
    }

    inline Float4x4 RotationY(float radians)
    {
        const float c = std::cos(radians);
        const float s = std::sin(radians);

        Float4x4 out{};
        out._11 = c; out._13 = -s;
        out._31 = s; out._33 = c;
        return out;
    }

    inline Float4x4 RotationZ(float radians)
    {
        const float c = std::cos(radians);
        const float s = std::sin(radians);

        Float4x4 out{};
        out._11 = c;  out._12 = s;
        out._21 = -s; out._22 = c;
        return out;
    }

    // Camera transform: world space -> view space, looking from `eye`
    // toward `at`. `up` need not be perpendicular to the view direction;
    // it only has to be non-parallel, since the basis is re-orthogonalised.
    inline Float4x4 LookAtLH(const Float3& eye, const Float3& at, const Float3& up)
    {
        const Float3 zaxis = Normalize(at - eye);
        const Float3 xaxis = Normalize(Cross(up, zaxis));
        const Float3 yaxis = Cross(zaxis, xaxis);

        Float4x4 out{};
        out._11 = xaxis.x; out._12 = yaxis.x; out._13 = zaxis.x;
        out._21 = xaxis.y; out._22 = yaxis.y; out._23 = zaxis.y;
        out._31 = xaxis.z; out._32 = yaxis.z; out._33 = zaxis.z;
        // The translation row is the eye position expressed in the new
        // basis, negated - rotating first and then translating.
        out._41 = -Dot(xaxis, eye);
        out._42 = -Dot(yaxis, eye);
        out._43 = -Dot(zaxis, eye);
        return out;
    }

    // Perspective projection from a vertical field of view.
    //
    // Note _34 = 1 and _44 = 0: that is what puts view-space z into the
    // w component, so the perspective divide happens. A matrix that looks
    // right but leaves _44 = 1 gives an orthographic projection.
    inline Float4x4 PerspectiveFovLH(float fovYRadians, float aspect, float nearZ, float farZ)
    {
        const float yScale = 1.0f / std::tan(fovYRadians * 0.5f);
        const float xScale = yScale / aspect;

        Float4x4 out{};
        out._11 = xScale;
        out._22 = yScale;
        out._33 = farZ / (farZ - nearZ);
        out._34 = 1.0f;
        out._43 = -nearZ * farZ / (farZ - nearZ);
        out._44 = 0.0f;
        return out;
    }

    inline Float4x4 Transpose(const Float4x4& in)
    {
        Float4x4 result{};
        for (int row{}; row < 4; ++row)
        {
            for (int col{}; col < 4; ++col) result.m[row][col] = in.m[col][row];
        }
        return result;
    }

    // Row-vector transform: v * M, with v.w = 1 (a position).
    inline Float3 TransformPoint(const Float3& v, const Float4x4& mtx)
    {
        return {
            v.x * mtx._11 + v.y * mtx._21 + v.z * mtx._31 + mtx._41,
            v.x * mtx._12 + v.y * mtx._22 + v.z * mtx._32 + mtx._42,
            v.x * mtx._13 + v.y * mtx._23 + v.z * mtx._33 + mtx._43
        };
    }

    // --- Planes ------------------------------------------------------
    // A plane is (a, b, c, d) with a*x + b*y + c*z + d = 0.

    inline Float4 NormalizePlane(const Float4& plane)
    {
        const float len = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
        return (len > kEpsilon) ? plane * (1.f / len) : Float4{};
    }

    // Signed distance from a point to a plane (XMPlaneDotCoord's job).
    constexpr float PlaneDotCoord(const Float4& plane, const Float3& point)
    {
        return plane.x * point.x + plane.y * point.y + plane.z * point.z + plane.w;
    }

    // --- Scalar helpers ----------------------------------------------

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
    inline bool fLessThan(float lhs, float rhs, float epsilon = kEpsilon) noexcept
    {
        return lhs - rhs < -epsilon;
    }
    inline bool fLessEqual(float lhs, float rhs, float epsilon = kEpsilon) noexcept
    {
        return lhs - rhs <= epsilon;
    }
    inline bool fGreaterThan(float lhs, float rhs, float epsilon = kEpsilon) noexcept
    {
        return lhs - rhs > epsilon;
    }
    inline bool fGreaterEqual(float lhs, float rhs, float epsilon = kEpsilon) noexcept
    {
        return lhs - rhs >= -epsilon;
    }
    inline bool Float3Equals(const Float3& lhs, const Float3& rhs, float epsilon = kEpsilon)
    {
        return std::abs(lhs.x - rhs.x) < epsilon
            and std::abs(lhs.y - rhs.y) < epsilon
            and std::abs(lhs.z - rhs.z) < epsilon;
    }

    // --- Shapes / culling --------------------------------------------

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
        Float3 position{};
        float radius{};
        uint32_t sliceCount{};
        uint32_t stackCount{};
    };
    struct SBoundAABB
    {
        Float3 min{};
        Float3 max{};
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
        explicit SFrustum(const Float4x4& viewProj);

        Float4 planes[6]{};

        bool TestBounds(const BoundingBox& bb) const
        {
            bool collides = true;

            std::visit(overloaded
            {
                [this, &collides](const SBoundSphere& sphere)
                {
                    for (const Float4& plane : planes)
                    {
                        if (PlaneDotCoord(plane, sphere.position) < -sphere.radius)
                        {
                            collides = false;
                            break;
                        }
                    }
                },
                [this, &collides](const SBoundAABB& aabb)
                {
                    for (const Float4& plane : planes)
                    {
                        // Pick the AABB corner furthest along the plane
                        // normal ("positive vertex"); if even that is
                        // behind the plane, the whole box is outside.
                        const Float3 pVertex{
                            (plane.x >= 0.f) ? aabb.max.x : aabb.min.x,
                            (plane.y >= 0.f) ? aabb.max.y : aabb.min.y,
                            (plane.z >= 0.f) ? aabb.max.z : aabb.min.z
                        };

                        if (PlaneDotCoord(plane, pVertex) < 0.f)
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

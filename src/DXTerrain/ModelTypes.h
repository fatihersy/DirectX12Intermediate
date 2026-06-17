#pragma once

namespace NSModel
{
    struct Vertex
    {
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT3 normal{};
        DirectX::XMFLOAT3 tangent{};
        DirectX::XMFLOAT3 bitangent{};
        DirectX::XMFLOAT2 texCoord{};
    };

    struct SDome {
        float radius{};
        UINT sliceCount{};
        UINT stackCount{};
    };
    struct SSphere {
        float radius{};
        UINT sliceCount{};
        UINT stackCount{};
    };
    struct SCube {
        float width{};
        float height{};
        float depth{};
    };
    struct SPlane {
        float width{};
        float depth{};
        UINT widthSubdivisions{};
        UINT depthSubdivisions{};
    };
    struct SCylinder {
        float topRadius{};
        float bottomRadius{};
        float height{};
        UINT sliceCount{};
        UINT stackCount{};
    };
    struct SCone {
        float bottomRadius{};
        float height{};
        UINT sliceCount{};
        UINT stackCount{};
    };

    enum class EModelFlag : uint32_t {
        UNDEFINED            = 0,
        PBR_MODEL            = 1 << 0,
        STATIC               = 1 << 1,
        DYNAMIC              = 1 << 2,
        ATMOSPHERE           = 1 << 3,
        GENERATE_ENV_CUBEMAP = 1 << 4,
        MAX                  = 1 << 5,
        Force32Bit = UINT32_MAX,
    };

    enum class EPrimitive : uint32_t {
        NONE,
        DOME,
        SPHERE,
        CUBE,
        PLANE,
        CYLINDER,
        CONE,
        MAX,
    };

    struct AddCtx
    {
        std::wstring_view name;
        DirectX::XMFLOAT3 position = {};
        float metallic = 0.f;
        float roughness = 0.f;
        float opacity = 1.f;
    };

    template<typename T>
    struct PrimitiveTraits {
        static constexpr EPrimitive type = EPrimitive::NONE;
    };

    template<>
    struct PrimitiveTraits<SDome> {
        PrimitiveTraits(SDome desc) : desc(desc) {};
        SDome desc{};
        static constexpr EPrimitive type = EPrimitive::DOME;
    };
    static_assert(offsetof(PrimitiveTraits<SDome>, desc) == 0);

    template<>
    struct PrimitiveTraits<SSphere> {
        PrimitiveTraits(SSphere desc) : desc(desc) {};
        SSphere desc{};
        static constexpr EPrimitive type = EPrimitive::SPHERE;
    };
    static_assert(offsetof(PrimitiveTraits<SSphere>, desc) == 0);

    template<>
    struct PrimitiveTraits<SCube> {
        PrimitiveTraits(SCube desc) : desc(desc) {};
        SCube desc{};
        static constexpr EPrimitive type = EPrimitive::CUBE;
    };
    static_assert(offsetof(PrimitiveTraits<SCube>, desc) == 0);

    template<>
    struct PrimitiveTraits<SPlane> {
        PrimitiveTraits(SPlane desc) : desc(desc) {};
        SPlane desc{};
        static constexpr EPrimitive type = EPrimitive::PLANE;
    };
    static_assert(offsetof(PrimitiveTraits<SPlane>, desc) == 0);

    template<>
    struct PrimitiveTraits<SCylinder> {
        PrimitiveTraits(SCylinder desc) : desc(desc) {};
        SCylinder desc{};
        static constexpr EPrimitive type = EPrimitive::CYLINDER;
    };
    static_assert(offsetof(PrimitiveTraits<SCylinder>, desc) == 0);

    template<>
    struct PrimitiveTraits<SCone> {
        PrimitiveTraits(SCone desc) : desc(desc) {};
        SCone desc{};
        static constexpr EPrimitive type = EPrimitive::CONE;
    };
    static_assert(offsetof(PrimitiveTraits<SCone>, desc) == 0);

    template<typename T> concept IsPrimitiveMesh = PrimitiveTraits<std::decay_t<T>>::type != EPrimitive::NONE;
}

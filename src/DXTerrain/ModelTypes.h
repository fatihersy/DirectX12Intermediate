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
        MODEL_FLAG_NONE = 0,
        MODEL_FLAG_NO_ENV_CUBEMAP = 1,
        MODEL_FLAG_MAX,
        MODEL_FLAG_Force32Bit = UINT32_MAX,
    };

    enum class ERegModelFlag : uint32_t {
        MODEL_FLAG_NONE = 0,
        MODEL_FLAG_UNSEEN_TO_ENV_CAPTURE = 1,
        MODEL_FLAG_MAX,
        MODEL_FLAG_Force32Bit = UINT32_MAX,
    };

    enum class EPrimitive : uint32_t {
        PRIMITIVE_TYPE_NONE,
        PRIMITIVE_TYPE_DOME,
        PRIMITIVE_TYPE_SPHERE,
        PRIMITIVE_TYPE_CUBE,
        PRIMITIVE_TYPE_PLANE,
        PRIMITIVE_TYPE_CYLINDER,
        PRIMITIVE_TYPE_CONE,
        PRIMITIVE_TYPE_MAX,
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
        static constexpr EPrimitive type = EPrimitive::PRIMITIVE_TYPE_NONE;
    };

    template<>
    struct PrimitiveTraits<SDome> {
        PrimitiveTraits(SDome desc) : desc(desc) {};
        SDome desc{};
        static constexpr EPrimitive type = EPrimitive::PRIMITIVE_TYPE_DOME;
    };
    static_assert(offsetof(PrimitiveTraits<SDome>, desc) == 0);

    template<>
    struct PrimitiveTraits<SSphere> {
        PrimitiveTraits(SSphere desc) : desc(desc) {};
        SSphere desc{};
        static constexpr EPrimitive type = EPrimitive::PRIMITIVE_TYPE_SPHERE;
    };
    static_assert(offsetof(PrimitiveTraits<SSphere>, desc) == 0);

    template<>
    struct PrimitiveTraits<SCube> {
        PrimitiveTraits(SCube desc) : desc(desc) {};
        SCube desc{};
        static constexpr EPrimitive type = EPrimitive::PRIMITIVE_TYPE_CUBE;
    };
    static_assert(offsetof(PrimitiveTraits<SCube>, desc) == 0);

    template<>
    struct PrimitiveTraits<SPlane> {
        PrimitiveTraits(SPlane desc) : desc(desc) {};
        SPlane desc{};
        static constexpr EPrimitive type = EPrimitive::PRIMITIVE_TYPE_PLANE;
    };
    static_assert(offsetof(PrimitiveTraits<SPlane>, desc) == 0);

    template<>
    struct PrimitiveTraits<SCylinder> {
        PrimitiveTraits(SCylinder desc) : desc(desc) {};
        SCylinder desc{};
        static constexpr EPrimitive type = EPrimitive::PRIMITIVE_TYPE_CYLINDER;
    };
    static_assert(offsetof(PrimitiveTraits<SCylinder>, desc) == 0);

    template<>
    struct PrimitiveTraits<SCone> {
        PrimitiveTraits(SCone desc) : desc(desc) {};
        SCone desc{};
        static constexpr EPrimitive type = EPrimitive::PRIMITIVE_TYPE_CONE;
    };
    static_assert(offsetof(PrimitiveTraits<SCone>, desc) == 0);

    template<typename T> concept IsPrimitiveMesh = PrimitiveTraits<std::decay_t<T>>::type != EPrimitive::PRIMITIVE_TYPE_NONE;

    struct SceneModelKey {
        uint32_t id = INT_MAX;
        size_t index = INT_MAX;
    };
    struct RegisterModelKey {
        uint32_t id = INT_MAX;
        size_t index = INT_MAX;
    };

    inline bool operator==(SceneModelKey& lhs, SceneModelKey& rhs) {
        return lhs.id == rhs.id and lhs.index == rhs.index;
    }
    inline bool operator!=(SceneModelKey& lhs, SceneModelKey& rhs) {
        return lhs.id != rhs.id or lhs.index != rhs.index;
    }
    inline bool operator==(RegisterModelKey& lhs, RegisterModelKey& rhs) {
        return lhs.id == rhs.id and lhs.index == rhs.index;
    }
    inline bool operator!=(RegisterModelKey& lhs, RegisterModelKey& rhs) {
        return lhs.id != rhs.id or lhs.index != rhs.index;
    }
}

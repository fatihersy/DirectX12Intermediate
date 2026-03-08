#pragma once

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
    PrimitiveTraits(SSphere desc) : desc(desc){};
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

#pragma once

struct frameConstants
{
    DirectX::XMFLOAT4X4 viewMatrix{};
    DirectX::XMFLOAT4X4 projectionMatrix{};
    DirectX::XMFLOAT4 lightDir{};
    DirectX::XMFLOAT4 lightColor{};
    DirectX::XMFLOAT3 camPos{};
    UINT PADDING_1{};
};
static_assert(sizeof(frameConstants) % 16 == 0);
static_assert(offsetof(frameConstants, PADDING_1) % 4 == 0);

union PaddedFrameConstants
{
    frameConstants constant;
    uint8_t bytes[D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT];
};
static_assert(sizeof(PaddedFrameConstants) == D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT * 1);

struct meshConstants
{
    DirectX::XMFLOAT4X4 worldMatrix;
    DirectX::XMFLOAT3X4 normalMatrix;
    DirectX::XMFLOAT4 baseColor;
    FLOAT metallic{};
    FLOAT roughness{};
    FLOAT opacity{};
    UINT textureFlags{};
};
static_assert(sizeof(meshConstants) % 16 == 0);
static_assert(offsetof(meshConstants, textureFlags) % 4 == 0);

union PaddedMeshConstants
{
    meshConstants constant;
    uint8_t bytes[D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT];
};
static_assert(sizeof(PaddedMeshConstants) == D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT * 1);

struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT3 tangent;
    DirectX::XMFLOAT3 bitangent;
    DirectX::XMFLOAT2 texCoord;
};

struct DrawContext {
    ID3D12GraphicsCommandList* cmdList;
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvGPUHandle;
    UINT srvDescriptorSize;
    UINT bufferIndex;
    UINT& bufferOffset;
    D3D12_GPU_VIRTUAL_ADDRESS meshConstantsGpuVirtualAddr;
    PaddedMeshConstants* meshConstantsCpuAddr;
};

struct SSphere {
    float radius;
    UINT sliceCount;
    UINT stackCount;
};
struct SCube {
    float width;
    float height;
    float depth;
};
struct SPlane {
    float width;
    float depth;
    UINT widthSubdivisions;
    UINT depthSubdivisions;
};
struct SCylinder {
    float topRadius;
    float bottomRadius;
    float height;
    UINT sliceCount;
    UINT stackCount;
};
struct SCone {
    float bottomRadius;
    float height;
    UINT sliceCount;
    UINT stackCount;
};

enum class EPrimitive : uint32_t {
    PRIMITIVE_TYPE_NONE,
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
struct PrimitiveTraits<SSphere> {
    PrimitiveTraits<SSphere>(SSphere desc) : desc(desc){};
    SSphere desc{};
    static constexpr EPrimitive type = EPrimitive::PRIMITIVE_TYPE_SPHERE;
};
static_assert(offsetof(PrimitiveTraits<SSphere>, desc) == 0);

template<>
struct PrimitiveTraits<SCube> {
    PrimitiveTraits<SCube>(SCube desc) : desc(desc) {};
    SCube desc{};
    static constexpr EPrimitive type = EPrimitive::PRIMITIVE_TYPE_CUBE;
};
static_assert(offsetof(PrimitiveTraits<SCube>, desc) == 0);

template<>
struct PrimitiveTraits<SPlane> {
    PrimitiveTraits<SPlane>(SPlane desc) : desc(desc) {};
    SPlane desc{};
    static constexpr EPrimitive type = EPrimitive::PRIMITIVE_TYPE_PLANE;
};
static_assert(offsetof(PrimitiveTraits<SPlane>, desc) == 0);

template<>
struct PrimitiveTraits<SCylinder> {
    PrimitiveTraits<SCylinder>(SCylinder desc) : desc(desc) {};
    SCylinder desc{};
    static constexpr EPrimitive type = EPrimitive::PRIMITIVE_TYPE_CYLINDER;
};
static_assert(offsetof(PrimitiveTraits<SCylinder>, desc) == 0);

template<>
struct PrimitiveTraits<SCone> {
    PrimitiveTraits<SCone>(SCone desc) : desc(desc) {};
    SCone desc{};
    static constexpr EPrimitive type = EPrimitive::PRIMITIVE_TYPE_CONE;
};
static_assert(offsetof(PrimitiveTraits<SCone>, desc) == 0);

template<typename T> concept IsPrimitiveMesh = PrimitiveTraits<std::decay_t<T>>::type != EPrimitive::PRIMITIVE_TYPE_NONE;

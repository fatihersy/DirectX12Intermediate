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
    D3D12_GPU_VIRTUAL_ADDRESS meshConstantsGpuVirtualAddr;
    PaddedMeshConstants* meshConstantsCpuAddr;
};

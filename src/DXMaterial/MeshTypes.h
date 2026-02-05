#pragma once

struct ConstantBuffer
{
    DirectX::XMFLOAT4X4 worldMatrix;
    DirectX::XMFLOAT4X4 viewMatrix;
    DirectX::XMFLOAT4X4 projectionMatrix;
    DirectX::XMFLOAT4 lightDir;
    DirectX::XMFLOAT4 lightColor;
};
static_assert(sizeof(ConstantBuffer) == 224);

union PaddedConstantBuffer
{
    ConstantBuffer constant;
    uint8_t bytes[D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT];
};
static_assert(sizeof(PaddedConstantBuffer) == D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT * 1);

struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT2 texCoord;
};

struct DrawContext {
    ID3D12GraphicsCommandList* cmdList;
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvGPUHandle;
    D3D12_GPU_VIRTUAL_ADDRESS baseGpuAddr;
    PaddedConstantBuffer* cbBufferCPU;
    ConstantBuffer cbParams;
    UINT frameBaseIndex;
    UINT srvDescriptorSize;
};

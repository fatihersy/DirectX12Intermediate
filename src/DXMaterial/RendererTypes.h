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

struct meshConstants
{
    DirectX::XMFLOAT4X4 worldMatrix{};
    DirectX::XMFLOAT3X4 normalMatrix{};
    DirectX::XMFLOAT4 baseColor{};
    FLOAT metallic{};
    FLOAT roughness{};
    FLOAT opacity{};
    UINT textureFlags{};
};
static_assert(sizeof(meshConstants) % 16 == 0);
static_assert(offsetof(meshConstants, textureFlags) % 4 == 0);

struct skyDomeConstants
{
    DirectX::XMFLOAT3 BetaR{};
    FLOAT PadR{};
    FLOAT BetaMScatter{};
    FLOAT BetaMExtinct{};
    FLOAT MieG{};
    FLOAT Pad0{};
    FLOAT HR{};
    FLOAT HM{};
    FLOAT Rg{};
    FLOAT Rt{};
    FLOAT SunIntensity{};
    DirectX::XMFLOAT3 SunDir{};
};
static_assert(sizeof(skyDomeConstants) % 16 == 0);
static_assert(offsetof(skyDomeConstants, SunDir) % 4 == 0);

namespace RendererTypes
{
    struct DepthStencilCreateDescription {
        DXGI_FORMAT format{};
        D3D12_DSV_FLAGS flags{};
        D3D12_DSV_DIMENSION dimention{};
        UINT width;
        UINT height;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
        ComPtr<ID3D12Resource2>& outDSV;
    };
}

namespace Descriptor
{
    struct Handle {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuAddr{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuAddr{};
        uint32_t index = UINT32_MAX;
        uint32_t amount{};
    };
    struct hOffset {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuAddr{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuAddr{};
        uint32_t index = UINT32_MAX;
    };
}

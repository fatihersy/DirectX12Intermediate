#pragma once

namespace NSDescriptor
{
    struct Handle
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuAddr{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuAddr{};
        uint32_t index = UINT32_MAX;
        uint32_t amount{};
    };

    struct Offset
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuAddr{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuAddr{};
        uint32_t index = UINT32_MAX;
    };
}

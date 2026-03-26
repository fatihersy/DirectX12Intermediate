#pragma once

namespace NSAllocator
{
    struct Ctx {
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddr{};
        void* cpuAddr = nullptr;

        template<typename T> T& As()
        {
            assert(cpuAddr);
            return * reinterpret_cast<T*>(cpuAddr);
        }

        explicit operator bool() const
        {
            return cpuAddr != nullptr;
        };
    };
};


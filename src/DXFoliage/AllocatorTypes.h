#pragma once

namespace NSAllocator
{
    struct Ctx {
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddr{};
        void* cpuAddr = nullptr;

        template<typename T> T& As()
        {
            ASSERT(cpuAddr != nullptr);
            return DE_REF(reinterpret_cast<T*>(cpuAddr));
        }

        explicit operator bool() const
        {
            return cpuAddr != nullptr;
        };
    };
}

#pragma once
#include "core/Defines.h"

#include "DescriptorTypes.h"

namespace NSDescriptor
{
    class IDescriptor
    {
        public:
            IDescriptor() {};
            IDescriptor(ID3D12Device14* device, LPCWSTR name,  D3D12_DESCRIPTOR_HEAP_TYPE type, UINT capacity, D3D12_DESCRIPTOR_HEAP_FLAGS flags);
            virtual ~IDescriptor();

            const bool Owns(Offset offset) const
            {
                const CD3DX12_CPU_DESCRIPTOR_HANDLE thisHeap(im_cpuStart, offset.index, im_descriptorSize);

                return thisHeap.ptr == offset.cpuAddr.ptr;
            }
            const bool Owns(Handle handle) const
            {
                const CD3DX12_CPU_DESCRIPTOR_HANDLE thisHeap(im_cpuStart, handle.index, im_descriptorSize);

                return thisHeap.ptr == handle.cpuAddr.ptr;
            }

            Offset At(uint32_t index) const
            {
                ASSERT(index < im_desc.NumDescriptors);

                return Offset {
                    .cpuAddr = CD3DX12_CPU_DESCRIPTOR_HANDLE(im_cpuStart, index, im_descriptorSize),
                    .gpuAddr = CD3DX12_GPU_DESCRIPTOR_HANDLE(im_gpuStart, index, im_descriptorSize),
                    .index = index
                };
            }

            const ID3D12DescriptorHeap* Raw() const
            {
                return im_heap.Get();
            }

            void Reset() {
                im_heap.Reset();
                im_device = nullptr;
            }

        protected:
            ID3D12Device14* im_device = nullptr;
            D3D12_DESCRIPTOR_HEAP_DESC im_desc{};
            uint32_t im_descriptorSize{};
            ComPtr<ID3D12DescriptorHeap> im_heap;
            D3D12_CPU_DESCRIPTOR_HANDLE im_cpuStart{};
            D3D12_GPU_DESCRIPTOR_HANDLE im_gpuStart{};
    };

    class StaticHeap : public IDescriptor
    {
        public:
            StaticHeap(){};
            StaticHeap(ID3D12Device14* device, LPCWSTR name, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT capacity, bool visible);
            ~StaticHeap();

            Handle Allocate(uint32_t amouth = 1u);
            void Free(Handle handle);

        private:
            std::vector<uint32_t> m_freeList;
    };

    class RingHeap : public IDescriptor
    {
        public:
            RingHeap() {};
            RingHeap(ID3D12Device14* device, LPCWSTR name, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT perFrameCapacity, uint32_t framesInFlight, uint32_t staticCapacity = 0u);
            ~RingHeap();

            Handle AllocateRing(uint32_t amount = 1u);
            Handle AllocateStatic(uint32_t amount = 1u);
            void FreeStatic(Handle handle);

            void BeginFrame(uint32_t frameIndex)
            {
                m_heapFrameIndexOffset = frameIndex * m_heapFrameCapacity;
                m_heapFrameEnd = m_heapFrameIndexOffset + m_heapFrameCapacity;
            }

        private:
            std::vector<uint32_t> m_freeList;

            uint32_t m_heapStaticAllocStart{};
            uint32_t m_heapFrameCapacity{};
            uint32_t m_heapFrameEnd{};
            uint32_t m_heapFrameIndexOffset{};
    };
}

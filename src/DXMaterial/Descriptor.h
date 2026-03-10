#pragma once

namespace Descriptor
{
    class IDescriptor
    {
    public:
        IDescriptor(){};
        IDescriptor(ID3D12Device14* device, LPCWSTR name, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT capacity, D3D12_DESCRIPTOR_HEAP_FLAGS flags);
        virtual ~IDescriptor();

        inline const ID3D12Device* GetDevice() const { return im_device; }
        inline const D3D12_DESCRIPTOR_HEAP_DESC GetDesc() const { return im_desc; }
        inline const uint32_t GetDescriptorSize() const { return im_descriptorSize; }

        inline const ID3D12DescriptorHeap* GetHeap() const { return im_heap.Get(); }
        inline const D3D12_CPU_DESCRIPTOR_HANDLE GetCpuStart() const { return im_cpuStart; }
        inline const D3D12_GPU_DESCRIPTOR_HANDLE GetGpuStart() const { return im_gpuStart; }

        inline const bool Validate(Handle handle) const
        {
            const CD3DX12_CPU_DESCRIPTOR_HANDLE thisHeap(im_cpuStart, handle.index, im_descriptorSize);

            return thisHeap.ptr == handle.cpuAddr.ptr;
        }

        inline bool At(uint32_t index, hOffset& offset) const
        {
            if (index >= im_desc.NumDescriptors) return false;
            
            offset.index = index;
            offset.cpuAddr = CD3DX12_CPU_DESCRIPTOR_HANDLE(im_cpuStart, index, im_descriptorSize);
            offset.gpuAddr = CD3DX12_GPU_DESCRIPTOR_HANDLE(im_gpuStart, index, im_descriptorSize);
            return true;
        }

        inline hOffset Offset(const Handle& from, uint32_t offset) const
        {
            assert(offset < from.amount && "Descriptor is out of slots");

            return {
                .cpuAddr = CD3DX12_CPU_DESCRIPTOR_HANDLE(from.cpuAddr, offset, GetDescriptorSize()),
                .gpuAddr = CD3DX12_GPU_DESCRIPTOR_HANDLE(from.gpuAddr, offset, GetDescriptorSize()),
                .index = from.index + offset,
            };
        }

    private:
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

        Handle Allocate(uint32_t amount = 1u);
        void Free(Handle handle);

    private:
        std::vector<uint32_t> m_freeList;
    };

    class RingHeap : public IDescriptor
    {
    public:
        RingHeap(){};
        RingHeap(ID3D12Device14* device, LPCWSTR name, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptorsPerFrame, uint32_t framesInFlight, uint32_t staticCapacity = 0u);
        ~RingHeap();

        Handle AllocateRing(uint32_t amount = 1u);
        Handle AllocateStatic(uint32_t amount = 1u);
        void FreeStatic(Handle handle);

        inline void BeginFrame(uint32_t frameIndex)
        {
            m_heapFrameIndexOffset = frameIndex * m_heapFrameCapacity;
            m_heapFrameEnd = m_heapFrameIndexOffset + m_heapFrameCapacity;
        }
        
    private:
        std::vector<uint32_t> m_freeList;

        uint32_t m_heapTotalCapacity{};

        uint32_t m_heapStaticAllocStart{};

        uint32_t m_heapFrameCapacity{};
        uint32_t m_heapFrameEnd{};
        uint32_t m_heapFrameIndexOffset{};
    };
}



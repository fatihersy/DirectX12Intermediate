#include "stdafx.h"
#include "Descriptor.h"

#include "DXSampleHelper.h"

using namespace NSDescriptor;

struct SsearchBlockResult
{
    int64_t handleIndexBegin{};
    int64_t vectorIndexBegin{};
    bool success{};
};

SsearchBlockResult GetFirstContiguousBlock(std::vector<uint32_t>& vec, uint32_t amount)
{
    if (vec.size() < amount) return {};

    SsearchBlockResult result{-1, -1, false};

    for (int64_t itr_000{}; static_cast<size_t>(itr_000) <= vec.size() - amount; itr_000++)
    {
        const uint32_t index = vec[itr_000];
        bool isContiguous{true};

        for (int64_t itr_111 = itr_000 + 1; itr_111 < itr_000 + amount; itr_111++)
        {
            if (vec[itr_111] == index + static_cast<uint32_t>(itr_111 - itr_000)) continue;

            itr_000 = itr_111 - 1;
            isContiguous = false;
            break;
        }

        if (isContiguous)
        {
            result.handleIndexBegin = static_cast<int64_t>(index);
            result.vectorIndexBegin = static_cast<int64_t>(itr_000);
            result.success = true;
            break;
        }
    }

    return result;
}

IDescriptor::IDescriptor(ID3D12Device14* device, LPCWSTR name,  D3D12_DESCRIPTOR_HEAP_TYPE type, UINT capacity, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
    ASSERT(device != nullptr);

    im_device = device;

    im_desc.Type = type;
    im_desc.NumDescriptors = capacity;
    im_desc.Flags = flags;

    ThrowIfFailed(device->CreateDescriptorHeap(&im_desc, IID_PPV_ARGS(&im_heap)));
    im_heap->SetName(name);

    im_descriptorSize = device->GetDescriptorHandleIncrementSize(type);
    im_cpuStart = im_heap->GetCPUDescriptorHandleForHeapStart();

    if (flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    {
        im_gpuStart = im_heap->GetGPUDescriptorHandleForHeapStart();
    }
}
IDescriptor::~IDescriptor()
{
    im_heap.Reset();
}

StaticHeap::StaticHeap(ID3D12Device14* device, LPCWSTR name, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT capacity, bool visible)
    : IDescriptor(device, name, type, capacity, visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE)
{
    m_freeList.resize(capacity);
    std::iota(m_freeList.begin(), m_freeList.end(), 0u);
}
StaticHeap::~StaticHeap()
{
    if (im_heap) im_heap.Reset();
}

Handle StaticHeap::Allocate(uint32_t amouth)
{
    SsearchBlockResult result = GetFirstContiguousBlock(m_freeList, amouth);
    ASSERT(result.success == true, "Insufficient free index");

    const std::vector<uint32_t>::const_iterator blockBegin = m_freeList.begin();
    ASSERT(blockBegin + amouth <= m_freeList.end());

    m_freeList.erase(blockBegin, blockBegin + amouth);

    return {
        .cpuAddr = CD3DX12_CPU_DESCRIPTOR_HANDLE(im_cpuStart, result.handleIndexBegin, im_descriptorSize),
        .gpuAddr = CD3DX12_GPU_DESCRIPTOR_HANDLE(im_gpuStart, result.handleIndexBegin, im_descriptorSize),
        .index = static_cast<uint32_t>(result.handleIndexBegin),
        .amouth = amouth
    };
}
void StaticHeap::Free(Handle handle)
{
    ASSERT(Owns(handle), "Invalid Handle");

    std::vector<uint32_t>::const_iterator pItr = std::lower_bound(m_freeList.begin(), m_freeList.end(), handle.index);

    ASSERT(pItr == m_freeList.end() or DE_REF(pItr) >= handle.index + handle.amouth, "Double free");

    for (uint32_t itr{}; itr < handle.amouth; itr++)
    {
        pItr = m_freeList.insert(pItr, handle.index + (handle.amouth - itr - 1u));
    }
}

RingHeap::RingHeap(ID3D12Device14* device, LPCWSTR name, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT perFrameCapacity, uint32_t framesInFlight, uint32_t staticCapacity)
    : IDescriptor(device, name, type, perFrameCapacity * framesInFlight + staticCapacity, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
{
    m_heapFrameCapacity = perFrameCapacity;
    m_heapStaticAllocStart = m_heapFrameCapacity * framesInFlight;

    m_freeList.resize(staticCapacity);

    std::iota(m_freeList.begin(), m_freeList.end(), 0u);
}
RingHeap::~RingHeap(){}
Handle RingHeap::AllocateRing(uint32_t amount)
{
    ASSERT(m_heapFrameIndexOffset + amount <= m_heapFrameEnd, "Out of bound");

    Handle handle = Handle
    {
        .cpuAddr = CD3DX12_CPU_DESCRIPTOR_HANDLE(im_cpuStart, m_heapFrameIndexOffset, im_descriptorSize),
        .gpuAddr = CD3DX12_GPU_DESCRIPTOR_HANDLE(im_gpuStart, m_heapFrameIndexOffset, im_descriptorSize),
        .index = m_heapFrameIndexOffset,
        .amouth = amount
    };

    m_heapFrameIndexOffset += amount;

    return handle;
}
Handle RingHeap::AllocateStatic(uint32_t amount)
{
    SsearchBlockResult result = GetFirstContiguousBlock(m_freeList, amount);
    ASSERT(result.success);

    const std::vector<uint32_t>::const_iterator blockBegin = m_freeList.begin() + static_cast<size_t>(result.vectorIndexBegin);
    ASSERT(blockBegin + amount <= m_freeList.end());

    m_freeList.erase(blockBegin, blockBegin + amount);

    return Handle
    {
        .cpuAddr = CD3DX12_CPU_DESCRIPTOR_HANDLE(im_cpuStart, result.handleIndexBegin, im_descriptorSize),
        .gpuAddr = CD3DX12_GPU_DESCRIPTOR_HANDLE(im_gpuStart, result.handleIndexBegin, im_descriptorSize),
        .index = static_cast<uint32_t>(result.handleIndexBegin),
        .amouth = amount
    };
}
void RingHeap::FreeStatic(Handle handle)
{
    ASSERT(Owns(handle));

    std::vector<uint32_t>::const_iterator pItr = std::lower_bound(m_freeList.begin(), m_freeList.end(), handle.index);

    ASSERT(pItr == m_freeList.end() or DE_REF(pItr) >= handle.index + handle.amouth, "Double free");

    for (uint32_t itr{}; itr < handle.amouth; itr++)
    {
        pItr = m_freeList.insert(pItr, handle.index + (handle.amouth - itr - 1u));
    }
}

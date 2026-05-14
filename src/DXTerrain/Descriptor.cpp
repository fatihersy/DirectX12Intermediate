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

IDescriptor::IDescriptor(ID3D12Device14* device, LPCWSTR name, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT capacity, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
    ASSERT(device);

    im_device = device;

    im_desc.Type = type;
    im_desc.NumDescriptors = capacity;
    im_desc.Flags = flags;

    ThrowIfFailed(device->CreateDescriptorHeap(&im_desc, IID_PPV_ARGS(&im_heap)));
    im_heap->SetName(name);

    im_descriptorSize = device->GetDescriptorHandleIncrementSize(im_desc.Type);
    im_cpuStart = im_heap->GetCPUDescriptorHandleForHeapStart();

    if (flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
        im_gpuStart = im_heap->GetGPUDescriptorHandleForHeapStart();
}

IDescriptor::~IDescriptor()
{
    im_heap.Reset();
}

StaticHeap::StaticHeap(ID3D12Device14* device, LPCWSTR name, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT capacity, bool visible)
    : IDescriptor(device, name, type, capacity, visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE)
{
    m_freeList.resize(capacity);

    for (size_t itr = 0; itr < capacity; itr++) m_freeList[itr] = static_cast<uint32_t>(itr);
}
StaticHeap::~StaticHeap(){}

Handle StaticHeap::Allocate(uint32_t amount)
{
    ASSERT(m_freeList.size() >= amount and "Insufficient free index");

    SsearchBlockResult result = GetFirstContiguousBlock(m_freeList, amount);
    ASSERT(result.success);

    const std::vector<uint32_t>::const_iterator blockBegin = m_freeList.begin() + static_cast<size_t>(result.vectorIndexBegin);
    ASSERT(blockBegin + amount <= m_freeList.end());

    m_freeList.erase(blockBegin, blockBegin + amount);

    return Handle{
        .cpuAddr = CD3DX12_CPU_DESCRIPTOR_HANDLE(im_cpuStart, result.handleIndexBegin, im_descriptorSize),
        .gpuAddr = CD3DX12_GPU_DESCRIPTOR_HANDLE(im_gpuStart, result.handleIndexBegin, im_descriptorSize),
        .index = static_cast<uint32_t>(result.handleIndexBegin),
        .amount = amount
    };
}

void StaticHeap::Free(Handle handle)
{
    ASSERT(Validate(Offset{ .cpuAddr = handle.cpuAddr, .index = handle.index }) and "Invalid handle");

    std::vector<uint32_t>::const_iterator pItr = std::lower_bound(m_freeList.begin(), m_freeList.end(), handle.index);

    ASSERT(pItr == m_freeList.end() or (*pItr) >= handle.index + handle.amount and "Double free");

    for (uint32_t itr{}; itr < handle.amount; itr++)
    {
        pItr = m_freeList.insert(pItr, handle.index + (handle.amount - itr - 1u));
    }
}

RingHeap::RingHeap(ID3D12Device14* device, LPCWSTR name, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT perFrameCapacity, uint32_t framesInFlight, uint32_t staticCapacity)
    : IDescriptor(device, name, type, perFrameCapacity * framesInFlight + staticCapacity, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
{
    m_heapFrameCapacity = perFrameCapacity;
    m_heapStaticAllocStart = m_heapFrameCapacity * framesInFlight;

    m_freeList.resize(staticCapacity);

    for (uint32_t itr = 0; static_cast<size_t>(itr) < m_freeList.size(); itr++) m_freeList[itr] = m_heapStaticAllocStart + itr;
}
RingHeap::~RingHeap(){}

Handle RingHeap::AllocateRing(uint32_t amount)
{
    ASSERT(m_heapFrameIndexOffset + amount <= m_heapFrameEnd and "Out of slots");

    Handle handle = Handle {
        .cpuAddr = CD3DX12_CPU_DESCRIPTOR_HANDLE(im_cpuStart, m_heapFrameIndexOffset, im_descriptorSize),
        .gpuAddr = CD3DX12_GPU_DESCRIPTOR_HANDLE(im_gpuStart, m_heapFrameIndexOffset, im_descriptorSize),
        .index = m_heapFrameIndexOffset,
        .amount = amount
    };

    m_heapFrameIndexOffset += amount;

    return handle;
}

Handle RingHeap::AllocateStatic(uint32_t amount)
{
    ASSERT(m_freeList.size() >= amount and "Insufficient free index");

    SsearchBlockResult result = GetFirstContiguousBlock(m_freeList, amount);
    ASSERT(result.success);

    const std::vector<uint32_t>::const_iterator blockBegin = m_freeList.begin() + static_cast<size_t>(result.vectorIndexBegin);
    ASSERT(blockBegin + amount <= m_freeList.end());

    m_freeList.erase(blockBegin, blockBegin + amount);

    return Handle {
        .cpuAddr = CD3DX12_CPU_DESCRIPTOR_HANDLE(im_cpuStart, result.handleIndexBegin, im_descriptorSize),
        .gpuAddr = CD3DX12_GPU_DESCRIPTOR_HANDLE(im_gpuStart, result.handleIndexBegin, im_descriptorSize),
        .index = static_cast<uint32_t>(result.handleIndexBegin),
        .amount = amount
    };
}

void RingHeap::FreeStatic(Handle handle)
{
    ASSERT(Validate(Offset{ .cpuAddr = handle.cpuAddr, .index = handle.index }) and "Invalid handle");

    std::vector<uint32_t>::const_iterator pItr = std::lower_bound(m_freeList.begin(), m_freeList.end(), handle.index);

    ASSERT(pItr == m_freeList.end() or (*pItr) >= handle.index + handle.amount and "Double free");

    for (uint32_t itr{}; itr < handle.amount; itr++)
    {
        pItr = m_freeList.insert(pItr, handle.index + (handle.amount - itr - 1u));
    }
}

#include "stdafx.h"
#include "Descriptor.h"

#include "DXSampleHelper.h"

using namespace Descriptor;

struct SsearchBlockResult {
    int64_t handleIndexBegin = -1;
    int64_t vectorIndexBegin = -1;
    bool success{};
};

SsearchBlockResult GetFirstContiguousBlock(std::vector<uint32_t>& vec, uint32_t amount)
{
    if (vec.size() < amount) return {};

    SsearchBlockResult result {-1, -1};

    for (int64_t itr_000 = 0; static_cast<size_t>(itr_000) <= vec.size() - amount; itr_000++)
    {
        const uint32_t index = vec[itr_000];
        bool isContiguous = true;

        for (int64_t itr_111 = 1; itr_111 < amount; itr_111++)
        {
            if (vec[itr_000 + itr_111] == index + static_cast<uint32_t>(itr_111)) continue;

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
    if (not device)
        throw std::runtime_error("Device is invalid");

    im_device = device;

    im_desc.NumDescriptors = capacity;
    im_desc.Type = type;
    im_desc.Flags = flags;
    ThrowIfFailed(im_device->CreateDescriptorHeap(&im_desc, IID_PPV_ARGS(&im_heap)));
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

    for (size_t itr{}; itr < capacity; itr++) m_freeList[itr] = static_cast<uint32_t>(itr);
}

StaticHeap::~StaticHeap()
{
    m_freeList.clear();
}

Handle StaticHeap::Allocate(uint32_t amount)
{
    assert(m_freeList.size() >= amount && "Insufficient free index");

    SsearchBlockResult result = GetFirstContiguousBlock(m_freeList, amount);
    if (not result.success) throw std::runtime_error("Cannot allocate a contiguous block");

    result.handleIndexBegin = std::clamp(
        result.handleIndexBegin,
        0ll,
        static_cast<int64_t>(std::numeric_limits<int>::max())
    );
    result.vectorIndexBegin = std::clamp(
        result.vectorIndexBegin,
        0ll,
        static_cast<int64_t>(std::numeric_limits<int>::max())
    );

    const std::vector<uint32_t>::iterator blockBegin = m_freeList.begin() + static_cast<uint32_t>(result.vectorIndexBegin);
    m_freeList.erase(blockBegin, blockBegin + amount);

    Handle handle{};
    handle.amount = amount;
    handle.index = static_cast<uint32_t>(result.handleIndexBegin);
    handle.cpuAddr = CD3DX12_CPU_DESCRIPTOR_HANDLE(GetCpuStart(), static_cast<INT>(result.handleIndexBegin), GetDescriptorSize());
    handle.gpuAddr = CD3DX12_GPU_DESCRIPTOR_HANDLE(GetGpuStart(), static_cast<INT>(result.handleIndexBegin), GetDescriptorSize());

    return handle;
}

void StaticHeap::Free(Handle handle)
{
    assert(Validate(handle) and "Invalid handle");

    std::vector<uint32_t>::iterator pItr = std::lower_bound(m_freeList.begin(), m_freeList.end(), handle.index);

    if (pItr != m_freeList.end() and (*pItr) < handle.index + handle.amount)
    {
        throw std::runtime_error("It is illegal to double free a descriptor allocation.");
    }

    for (uint32_t i = 0u; i < handle.amount; i++)
    {
        pItr = m_freeList.insert(pItr, handle.index + (handle.amount - 1u - i));
    }
}

RingHeap::RingHeap(ID3D12Device14* device, LPCWSTR name, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptorsPerFrame, uint32_t framesInFlight, uint32_t staticCapacity)
    : IDescriptor(device, name, type, descriptorsPerFrame * framesInFlight + staticCapacity, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
{
    m_heapTotalCapacity = GetDesc().NumDescriptors;
    m_heapFrameCapacity = descriptorsPerFrame;

    m_heapStaticAllocStart = descriptorsPerFrame * framesInFlight;

    m_freeList.resize(staticCapacity);

    for (uint32_t itr{}; itr < staticCapacity; itr++) m_freeList[itr] = m_heapStaticAllocStart + itr;
}
RingHeap::~RingHeap()
{
}

Handle RingHeap::AllocateRing(uint32_t amount)
{
    assert(m_heapFrameIndexOffset + amount <= m_heapFrameEnd && "Descriptor is out of slots");

    Handle handle = {
        .cpuAddr = CD3DX12_CPU_DESCRIPTOR_HANDLE(GetCpuStart(), m_heapFrameIndexOffset, GetDescriptorSize()),
        .gpuAddr = CD3DX12_GPU_DESCRIPTOR_HANDLE(GetGpuStart(), m_heapFrameIndexOffset, GetDescriptorSize()),
        .index = m_heapFrameIndexOffset,
        .amount = amount
    };

    m_heapFrameIndexOffset += amount;

    return handle;
}

Handle RingHeap::AllocateStatic(uint32_t amount)
{
    assert(m_freeList.size() > amount && "Insufficient free index");

    SsearchBlockResult result = GetFirstContiguousBlock(m_freeList, amount);
    if (not result.success) throw std::runtime_error("Cannot allocate a contiguous block");

    const std::vector<uint32_t>::iterator blockBegin = m_freeList.begin() + result.vectorIndexBegin;
    m_freeList.erase(blockBegin, blockBegin + amount);

    return {
        .cpuAddr = CD3DX12_CPU_DESCRIPTOR_HANDLE(GetCpuStart(), result.handleIndexBegin, GetDescriptorSize()),
        .gpuAddr = CD3DX12_GPU_DESCRIPTOR_HANDLE(GetGpuStart(), result.handleIndexBegin, GetDescriptorSize()),
        .index = static_cast<uint32_t>(result.handleIndexBegin),
        .amount = amount
    };
}

void RingHeap::FreeStatic(Handle handle)
{
    assert(Validate({ .cpuAddr = handle.cpuAddr, .index = handle.index}) and "Invalid handle");

    std::vector<uint32_t>::const_iterator pItr = std::lower_bound(m_freeList.begin(), m_freeList.end(), handle.index);

    if (pItr != m_freeList.end() and (*pItr) < handle.index + handle.amount)
    {
        throw std::runtime_error("It is illegal to double free a descriptor allocation.");
    }

    for (uint32_t i = 0u; i < handle.amount; i++) {
        pItr = m_freeList.insert(pItr, handle.index + (handle.amount - 1u - i));
    }
}

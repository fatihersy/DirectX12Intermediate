#include "stdafx.h"
#include "Allocator.h"

ConstBuffAlloc::ConstBuffAlloc(ID3D12Device* device, size_t totalSize, UINT frameCount)
{
    m_blobTotalSize = totalSize;
    m_blobFrameSize = totalSize / frameCount;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = m_blobTotalSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_bufferDefault)
    );

    m_bufferDefault->Map(0, nullptr, &m_cpuAddr);
    m_gpuAddr = m_bufferDefault->GetGPUVirtualAddress();
}

ConstBuffAlloc::~ConstBuffAlloc()
{
    if (m_bufferDefault)
    {
        m_bufferDefault->Unmap(0, nullptr);
        m_bufferDefault.Reset();
    }
}

ConstBuffAlloc::ConstBuffAlloc(ConstBuffAlloc&& other) noexcept
    : m_bufferDefault(std::move(other.m_bufferDefault))
    , m_gpuAddr(other.m_gpuAddr)
    , m_cpuAddr(other.m_cpuAddr)
    , m_blobTotalSize(other.m_blobTotalSize)
    , m_blobFrameSize(other.m_blobFrameSize)
    , m_blobFrameEnd(other.m_blobFrameEnd)
    , m_blobOffset(other.m_blobOffset)
{
    other.m_gpuAddr = {};
    other.m_cpuAddr = nullptr;
    other.m_blobTotalSize = 0;
    other.m_blobFrameSize = 0;
    other.m_blobFrameEnd = 0;
    other.m_blobOffset = 0;
}

ConstBuffAlloc& ConstBuffAlloc::operator=(ConstBuffAlloc&& other) noexcept
{
    if (this != &other)
    {
        if (m_bufferDefault)
        {
            m_bufferDefault->Unmap(0, nullptr);
        }

        m_bufferDefault = std::move(other.m_bufferDefault);
        m_gpuAddr = other.m_gpuAddr;
        m_cpuAddr = other.m_cpuAddr;
        m_blobTotalSize = other.m_blobTotalSize;
        m_blobFrameSize = other.m_blobFrameSize;
        m_blobFrameEnd = other.m_blobFrameEnd;
        m_blobOffset = other.m_blobOffset;

        other.m_gpuAddr = {};
        other.m_cpuAddr = nullptr;
        other.m_blobTotalSize = 0;
        other.m_blobFrameSize = 0;
        other.m_blobFrameEnd = 0;
        other.m_blobOffset = 0;
    }
    return *this;
}

Allocator::AllocCtx ConstBuffAlloc::Allocate(size_t size)
{
    if (not m_cpuAddr or not m_bufferDefault)
    {
        throw std::runtime_error("Allocator has invalid address");
    }
    constexpr size_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    size_t aligned = (m_blobOffset + alignment - 1) & ~(alignment - 1);

    if (aligned + size >= m_blobFrameEnd)
    {
        throw std::runtime_error("Frame Overflow");
    }

    Allocator::AllocCtx ctx{};
    ctx.gpuAddr = m_gpuAddr + aligned;
    ctx.cpuAddr = static_cast<uint8_t*>(m_cpuAddr) + aligned;

    m_blobOffset = aligned + size;
    return ctx;
}

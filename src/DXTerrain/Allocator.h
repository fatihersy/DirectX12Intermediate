#pragma once

class ConstantAllocator
{
public:
    ConstantAllocator() {};
    ConstantAllocator(ID3D12Device14* device, size_t totalSize, UINT framesInFlight);
    ~ConstantAllocator();

    ConstantAllocator(const ConstantAllocator&) = delete;
    ConstantAllocator& operator=(const ConstantAllocator&) = delete;
    ConstantAllocator(ConstantAllocator&& other) noexcept;
    ConstantAllocator& operator=(ConstantAllocator&& other) noexcept;

    NSAllocator::Ctx Allocate(size_t size);

    void BeginFrame(UINT frameIndex)
    {
        m_blobOffset = frameIndex * m_blobFrameSize;
        m_blobFrameEnd = m_blobOffset + m_blobFrameSize;
    }

private:
    ComPtr<ID3D12Resource> m_bufferDefault;
    D3D12_GPU_VIRTUAL_ADDRESS m_gpuAddr{};
    void* m_cpuAddr = nullptr;

    size_t m_blobTotalSize{};
    size_t m_blobFrameSize{};
    size_t m_blobFrameEnd{};
    size_t m_blobOffset{};
};


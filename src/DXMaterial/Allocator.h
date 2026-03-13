#pragma once

class ConstBuffAlloc
{
public:
    ConstBuffAlloc() {};
    ConstBuffAlloc(ID3D12Device* device, size_t totalSize, UINT frameCount);
    ~ConstBuffAlloc();

    ConstBuffAlloc(const ConstBuffAlloc&) = delete;
    ConstBuffAlloc& operator=(const ConstBuffAlloc&) = delete;
    ConstBuffAlloc(ConstBuffAlloc&& other) noexcept;
    ConstBuffAlloc& operator=(ConstBuffAlloc&& other) noexcept;

    Allocator::AllocCtx Allocate(size_t size);

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

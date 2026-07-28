#pragma once

#include <cstddef>
#include <cstdint>

// A block of GPU memory: vertex data, index data, or small per-draw
// constants. Mirrors what ConstantAllocator/the ad-hoc vertex buffer in
// Renderer.cpp already do (a default-heap-or-mappable-upload-heap
// resource), just without a concrete D3D12 type in the public surface.
namespace NSRHI
{
    enum class EBufferUsage : uint8_t
    {
        Vertex,
        Index,
        Constant, // small, frequently-updated per-draw/per-frame data
        Upload,   // CPU-writable staging buffer, source for a CopyBuffer/CopyBufferToTexture
    };

    struct BufferDesc
    {
        size_t sizeBytes{};
        EBufferUsage usage{ EBufferUsage::Vertex };

        // true = persistently CPU-mappable (upload/constant buffers);
        // false = GPU-only memory (vertex/index buffers that were
        // uploaded once via a copy and never touched by the CPU again).
        bool cpuVisible{ false };
    };

    class IBuffer
    {
    public:
        virtual ~IBuffer() = default;

        virtual size_t Size() const = 0;

        // Only valid when created with cpuVisible = true.
        virtual void* Map() = 0;
        virtual void Unmap() = 0;
    };
}

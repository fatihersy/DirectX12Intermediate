#include "stdafx.h"
#include "Allocator.h"

#include "Logger.h"

namespace NSAllocator
{
    ConstantAllocator::ConstantAllocator(NSRHI::IBuffer& buffer, uint32_t framesInFlight)
        : m_buffer(&buffer)
    {
        ASSERT(framesInFlight > 0);
        ASSERT(buffer.Size() > NSRHI::kConstantBufferWindowBytes,
            "Constant buffer smaller than its own descriptor window");

        // The tail window is slack for the LAST allocation's descriptor
        // range (Vulkan: dynamicOffset + range must stay inside the
        // buffer), so the rings divide what remains.
        m_frameSize = (buffer.Size() - NSRHI::kConstantBufferWindowBytes) / framesInFlight;

        // Persistently mapped for the allocator's lifetime; both backends'
        // upload buffers keep the mapping at zero cost (VulkanBuffer maps
        // at creation, Unmap is a no-op) and DXTerrain's allocator held
        // its mapping open the same way.
        m_cpuBase = static_cast<uint8_t*>(buffer.Map());
        ASSERT(m_cpuBase, "Constant buffer must be cpuVisible");
    }

    void ConstantAllocator::BeginFrame(uint32_t frameIndex)
    {
        ASSERT(m_buffer, "BeginFrame on an empty ConstantAllocator");

        m_offset = frameIndex * m_frameSize;
        m_frameEnd = m_offset + m_frameSize;
    }

    Ctx ConstantAllocator::Allocate(size_t size)
    {
        ASSERT(m_buffer and m_cpuBase);
        ASSERT(m_frameEnd > 0, "Allocate before the first BeginFrame");
        ASSERT(size > 0 and size <= NSRHI::kConstantBufferWindowBytes,
            "A constant struct must fit the descriptor window");

        // DXTerrain's exact alignment line, constant renamed.
        constexpr size_t alignment = NSRHI::kConstantBufferAlignment;
        const size_t aligned = (m_offset + alignment - 1) & ~(alignment - 1);

        if (aligned + size > m_frameEnd)
        {
            // Loud and unmissable, like the descriptor ring: the fix is a
            // bigger buffer (or Diligent-style growth, adopted the day
            // this actually fires — see the ConstantAllocator report).
            g_FError("ConstantAllocator: frame region exhausted (%zu bytes/frame)", m_frameSize);
            return Ctx{};
        }

        Ctx ctx{};
        ctx.cpuAddr = m_cpuBase + aligned;
        ctx.offsetBytes = aligned;

        m_offset = aligned + size;
        return ctx;
    }
}

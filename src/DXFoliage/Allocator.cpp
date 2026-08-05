#include "stdafx.h"
#include "Allocator.h"

#include "Logger.h"

namespace NSAllocator
{
    RingAllocator::RingAllocator(NSRHI::IBuffer& buffer, uint32_t framesInFlight,
                                 size_t alignment, size_t tailSlack)
        : m_buffer(&buffer)
        , m_alignment(alignment)
    {
        ASSERT(framesInFlight > 0);
        ASSERT(alignment > 0 and (alignment & (alignment - 1)) == 0,
            "Alignment must be a power of two - the round-up below assumes it");
        ASSERT(buffer.Size() > tailSlack, "Buffer smaller than its own tail slack");

        m_frameSize = (buffer.Size() - tailSlack) / framesInFlight;
        ASSERT(m_frameSize > 0, "No room left per frame after tail slack");

        // With slack, that slack IS the cap (it exists so the last
        // allocation's fixed descriptor range stays in-buffer). Without,
        // one allocation simply cannot outgrow its frame region.
        m_maxAllocation = tailSlack > 0 ? tailSlack : m_frameSize;

        // Persistently mapped for the allocator's lifetime; both backends'
        // upload buffers keep the mapping at zero cost (VulkanBuffer maps
        // at creation, Unmap is a no-op) and DXTerrain's allocator held
        // its mapping open the same way.
        m_cpuBase = static_cast<uint8_t*>(buffer.Map());
        ASSERT(m_cpuBase, "Ring buffer must be cpuVisible");
    }

    void RingAllocator::BeginFrame(uint32_t frameIndex)
    {
        ASSERT(m_buffer, "BeginFrame on an empty RingAllocator");

        m_offset = frameIndex * m_frameSize;
        m_frameEnd = m_offset + m_frameSize;
    }

    Ctx RingAllocator::Allocate(size_t size)
    {
        ASSERT(m_buffer and m_cpuBase);
        ASSERT(m_frameEnd > 0, "Allocate before the first BeginFrame");
        ASSERT(size > 0 and size <= m_maxAllocation, "Allocation exceeds this ring's per-allocation cap");

        // DXTerrain's exact alignment line, alignment now a member.
        const size_t aligned = (m_offset + m_alignment - 1) & ~(m_alignment - 1);

        if (aligned + size > m_frameEnd)
        {
            // Loud and unmissable, like the descriptor ring: the fix is a
            // bigger buffer (or Diligent-style growth, adopted the day
            // this actually fires — see the ConstantAllocator report).
            g_FError("RingAllocator: frame region exhausted (%zu bytes/frame, wanted %zu)",
                m_frameSize, size);
            return Ctx{};
        }

        Ctx ctx{};
        ctx.cpuAddr = m_cpuBase + aligned;
        ctx.offsetBytes = aligned;

        m_offset = aligned + size;
        return ctx;
    }
}

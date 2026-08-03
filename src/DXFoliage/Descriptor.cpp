#include "stdafx.h"
#include "Descriptor.h"

#include "Logger.h"

#include <algorithm>

namespace NSDescriptor
{
    namespace
    {
        // Finds `amount` CONSECUTIVE indices in a sorted free list and
        // returns the position of the run's first element, or SIZE_MAX.
        // Same job as DXTerrain's GetFirstContiguousBlock: multi-slot
        // handles must be contiguous because the shader reaches slot N of
        // a range as g_textures[handle.index + N].
        size_t FindContiguousRun(const std::vector<uint32_t>& sortedFree, uint32_t amount)
        {
            if (sortedFree.size() < amount) return SIZE_MAX;
            if (amount == 1) return 0;  // any free slot will do

            size_t runStart = 0;
            for (size_t i = 1; i < sortedFree.size(); ++i)
            {
                if (sortedFree[i] != sortedFree[i - 1] + 1) runStart = i;
                if (i - runStart + 1 == amount) return runStart;
            }
            return SIZE_MAX;
        }

        // Sorted-insert keeps the list ascending so FindContiguousRun can
        // detect adjacency by comparing neighbours.
        void InsertSorted(std::vector<uint32_t>& sortedFree, uint32_t index)
        {
            sortedFree.insert(
                std::lower_bound(sortedFree.begin(), sortedFree.end(), index), index);
        }

        NSRHI::DescriptorHandle TakeRun(std::vector<uint32_t>& sortedFree,
                                        size_t runStart, uint32_t amount, uint32_t heapId)
        {
            const uint32_t first = sortedFree[runStart];
            sortedFree.erase(sortedFree.begin() + static_cast<ptrdiff_t>(runStart),
                             sortedFree.begin() + static_cast<ptrdiff_t>(runStart + amount));
            return NSRHI::DescriptorHandle{ .index = first, .amount = amount, .heapId = heapId };
        }
    }

    // --- StaticHeap ---

    StaticHeap::StaticHeap(NSRHI::IDescriptorHeap& heap)
        : m_heap(&heap)
    {
        m_freeList.reserve(heap.Capacity());
        for (uint32_t i = 0; i < heap.Capacity(); ++i) m_freeList.push_back(i);
    }

    NSRHI::DescriptorHandle StaticHeap::Allocate(uint32_t amount)
    {
        ASSERT(m_heap and amount > 0);

        const size_t run = FindContiguousRun(m_freeList, amount);
        if (run == SIZE_MAX)
        {
            g_FError("StaticHeap: no contiguous run of %u slots (heap %u)", amount, m_heap->HeapId());
            return NSRHI::DescriptorHandle{};
        }
        return TakeRun(m_freeList, run, amount, m_heap->HeapId());
    }

    void StaticHeap::Free(NSRHI::DescriptorHandle handle)
    {
        if (not handle.IsValid()) return;
        ASSERT(m_heap and m_heap->Owns(handle), "Handle does not belong to this heap");

        // See the in-flight contract in the header: only legal when no
        // pending frame still reads these slots.
        for (uint32_t i = 0; i < handle.amount; ++i)
        {
            InsertSorted(m_freeList, handle.index + i);
        }
    }

    // --- RingHeap ---

    RingHeap::RingHeap(NSRHI::IDescriptorHeap& heap, uint32_t perFrameCapacity,
                       uint32_t framesInFlight, uint32_t staticCapacity)
        : m_heap(&heap)
        , m_staticStart(perFrameCapacity * framesInFlight)
        , m_frameCapacity(perFrameCapacity)
    {
        ASSERT(perFrameCapacity > 0 and framesInFlight > 0);
        ASSERT(m_staticStart + staticCapacity == heap.Capacity(),
            "RingHeap regions must exactly cover the heap: perFrame * frames + static == capacity");

        m_freeList.reserve(staticCapacity);
        for (uint32_t i = 0; i < staticCapacity; ++i) m_freeList.push_back(m_staticStart + i);

        // Usable before the first BeginFrame (load-time code allocating
        // statics only); ring allocation before BeginFrame trips the
        // m_frameEnd == 0 assert in AllocateRing, which is the right error.
    }

    void RingHeap::BeginFrame(uint32_t frameIndex)
    {
        ASSERT(m_heap and (frameIndex + 1) * m_frameCapacity <= m_staticStart,
            "Frame index outside the ring region");

        m_frameOffset = frameIndex * m_frameCapacity;
        m_frameEnd = m_frameOffset + m_frameCapacity;
    }

    NSRHI::DescriptorHandle RingHeap::AllocateRing(uint32_t amount)
    {
        ASSERT(m_heap and amount > 0);
        ASSERT(m_frameEnd > 0, "AllocateRing before the first BeginFrame");

        if (m_frameOffset + amount > m_frameEnd)
        {
            g_FError("RingHeap: frame ring exhausted (%u per frame, heap %u)",
                m_frameCapacity, m_heap->HeapId());
            return NSRHI::DescriptorHandle{};
        }

        NSRHI::DescriptorHandle handle{
            .index = m_frameOffset, .amount = amount, .heapId = m_heap->HeapId() };
        m_frameOffset += amount;
        return handle;
    }

    NSRHI::DescriptorHandle RingHeap::AllocateStatic(uint32_t amount)
    {
        ASSERT(m_heap and amount > 0);

        const size_t run = FindContiguousRun(m_freeList, amount);
        if (run == SIZE_MAX)
        {
            g_FError("RingHeap: no contiguous static run of %u slots (heap %u)",
                amount, m_heap->HeapId());
            return NSRHI::DescriptorHandle{};
        }
        return TakeRun(m_freeList, run, amount, m_heap->HeapId());
    }

    void RingHeap::FreeStatic(NSRHI::DescriptorHandle handle)
    {
        if (not handle.IsValid()) return;
        ASSERT(m_heap and m_heap->Owns(handle), "Handle does not belong to this heap");
        ASSERT(handle.index >= m_staticStart, "FreeStatic on a ring-region handle");

        for (uint32_t i = 0; i < handle.amount; ++i)
        {
            InsertSorted(m_freeList, handle.index + i);
        }
    }
}

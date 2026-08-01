#include "stdafx.h"

// VMA ships as a header-only library whose implementation is guarded by a
// macro, so exactly one translation unit in the program must define this.
// That is the only reason this file exists.
#define VMA_IMPLEMENTATION

// VMA's implementation triggers a great deal of warning noise that is not
// ours to fix, and this project builds with fatalwarnings "All".
#if defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wall"
    #pragma GCC diagnostic ignored "-Wextra"
#endif

#include "VulkanMemory.h"

#if defined(__clang__)
    #pragma clang diagnostic pop
#elif defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif

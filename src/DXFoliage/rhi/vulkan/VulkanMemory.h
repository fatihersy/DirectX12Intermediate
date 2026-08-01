#pragma once

// VMA's single header, with the configuration this project needs applied
// in one place. Include this rather than <vk_mem_alloc.h> directly, or the
// defines below won't be consistent across translation units.
//
// Why VMA at all: Vulkan makes the application own GPU memory. Every
// resource otherwise needs create / query requirements / choose a memory
// type / allocate / bind, and each step is somewhere to get alignment or
// memory-type flags wrong. vmaCreateBuffer collapses all five into one
// call, and sub-allocates from a few large blocks rather than asking the
// driver per resource.
//
// The DX12 mapping, since it is easy to get backwards: one
// vkAllocateMemory per resource is CreateCommittedResource per resource.
// VMA is CreatePlacedResource into heaps it manages for you - committed's
// ergonomics with placed's efficiency.

// Must match the apiVersion in VulkanDevice::CreateInstance. VMA gates
// features like the dedicated-allocation and buffer-device-address paths
// on this, and silently assumes 1.0 if it is not set.
#define VMA_VULKAN_VERSION 1003000

#define VK_USE_PLATFORM_WAYLAND_KHR
#include <vk_mem_alloc.h>

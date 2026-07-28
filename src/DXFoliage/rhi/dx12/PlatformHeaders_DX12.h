#pragma once

// DirectX 12 headers. Included only by the DX12 backend (rhi/dx12/), NOT
// by stdafx.h — so Windows platform code and the neutral layers never see
// d3d12.h, and the Vulkan backend is unaffected by any of it.
//
// DirectX does need Windows types (HWND, HRESULT, ComPtr...), so this
// pulls in the Win32 header; the dependency only runs in that direction.
//
// Note this is deliberately outside the precompiled header: DX12 sources
// pay for parsing d3dx12.h themselves rather than every translation unit
// in the project paying for it.

#include "platform/win32/PlatformHeaders_Win32.h"

#include <directx/d3dx12.h>
#include <directx/d3d12shader.h>
#include <dxgi1_6.h>
#include "dxcapi.h"

#include "DirectXTypes.h"

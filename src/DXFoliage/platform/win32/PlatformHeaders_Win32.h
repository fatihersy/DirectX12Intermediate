#pragma once

// Windows platform headers — no DirectX. Pulled in by stdafx.h under
// D12F_OS_WINDOWS, so every Windows translation unit gets them, and by
// the Win32 platform backend directly.
//
// DirectX headers deliberately live in PlatformHeaders_DX12.h instead:
// windowing/input code has no business seeing d3d12.h, and keeping them
// apart means the graphics API can be swapped without touching this file.
// (The reverse isn't true — DirectX does need Windows types, so
// PlatformHeaders_DX12.h includes this one.)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <shellapi.h>

// WRL is COM infrastructure rather than a DirectX header — it backs the
// ComPtr alias in core/Defines.h, which is why it sits on this side.
#include <wrl.h>

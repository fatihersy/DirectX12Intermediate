#pragma once

// Standard library only — safe to include on every platform this project
// targets. Anything Windows- or DirectX-specific lives in
// PlatformHeaders_Win32.h instead, pulled in below only when actually
// building for Windows, so a Linux compiler never has to look at it.
#include <system_error>
#include <string>
#include <vector>
// <atomic> (EntityTypes.h) and <cmath> (core/Math.h) used to arrive
// transitively via windows.h — they have to be explicit now that the
// neutral build doesn't include it.
#include <atomic>
#include <cmath>
#include <map>
#include <format>
#include <array>
#include <filesystem>
#include <cstdio>
#include <memory>
#include <algorithm>
#include <numeric>
#include <functional>
#include <variant>
#include <type_traits>
#include <stdexcept>
#include <any>
#include <unordered_map>
#include <optional>
#include <bitset>
#include <typeinfo>
#include <cstddef>
#include <fstream>
#include <queue>

#include <libassert/assert.hpp>

// D12F_OS_WINDOWS / D12F_OS_LINUX are defined per-project by mox_project()
// in scripts/libmox.lua, based on the target OS the build is configured
// for — not the host OS this is compiled on.
#if defined(D12F_OS_WINDOWS)
    #include "platform/win32/PlatformHeaders_Win32.h"
#endif

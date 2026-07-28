#pragma once

#include <filesystem>

// Small OS services that aren't windowing or input, but still need a
// per-OS implementation. Implemented in platform/win32/ and
// platform/linux/ — note those two aren't symmetric: "win32" is both the
// OS API and the windowing system on Windows, whereas on Linux the OS
// layer (here) and the windowing system (platform/wayland/) are separate
// concerns.
namespace NSPlatform
{
    // Directory containing the running executable. Used to resolve asset
    // and shader paths relative to the binary rather than the CWD.
    std::filesystem::path GetExecutableDirectory();
}

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

    // Per-user directories, already suffixed with the project name, for
    // things the app writes rather than ships: saves, settings, caches.
    //
    // Writing beside the executable works for a demo run from a build
    // tree and is wrong for anything installed - /usr/bin is not writable,
    // and two users would share one file. Linux follows the XDG base
    // directory spec (XDG_DATA_HOME and friends, with the documented
    // fallbacks); Windows uses APPDATA / LOCALAPPDATA.
    //
    // The directory is NOT created; callers that write should
    // std::filesystem::create_directories first.
    std::filesystem::path GetUserDataDirectory();    // saves, generated content
    std::filesystem::path GetUserConfigDirectory();  // settings
    std::filesystem::path GetUserCacheDirectory();   // regenerable (shader cache)
}

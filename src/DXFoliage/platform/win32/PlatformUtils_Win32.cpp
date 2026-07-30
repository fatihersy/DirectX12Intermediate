#include "stdafx.h"
#include "platform/PlatformUtils.h"

namespace NSPlatform
{
    namespace
    {
        // Windows has no XDG split. APPDATA roams with the user across
        // machines in a domain; LOCALAPPDATA does not, which is what
        // caches want - a roaming shader cache would be copied over the
        // network for no benefit.
        std::filesystem::path KnownDir(const wchar_t* envVar)
        {
            WCHAR buffer[MAX_PATH]{};
            const DWORD size = GetEnvironmentVariableW(envVar, buffer, MAX_PATH);
            if (size == 0 or size >= MAX_PATH)
            {
                return std::filesystem::current_path();
            }

            return std::filesystem::path(buffer) / PROJECT_NAME;
        }
    }

    std::filesystem::path GetUserDataDirectory()
    {
        return KnownDir(L"APPDATA");
    }

    std::filesystem::path GetUserConfigDirectory()
    {
        // Same location as data: Windows convention keeps settings and
        // saves together under the app's APPDATA folder rather than
        // splitting them the way XDG does.
        return KnownDir(L"APPDATA");
    }

    std::filesystem::path GetUserCacheDirectory()
    {
        return KnownDir(L"LOCALAPPDATA");
    }

    std::filesystem::path GetExecutableDirectory()
    {
        // Replaces DXSampleHelper.h's GetExecutablePath(), which wrote
        // into a caller-supplied WCHAR buffer and truncated at the last
        // backslash by hand.
        WCHAR path[MAX_PATH]{};
        const DWORD size = GetModuleFileNameW(nullptr, path, MAX_PATH);
        if (size == 0 or size == MAX_PATH)
        {
            return std::filesystem::current_path();
        }

        return std::filesystem::path(path).parent_path();
    }
}

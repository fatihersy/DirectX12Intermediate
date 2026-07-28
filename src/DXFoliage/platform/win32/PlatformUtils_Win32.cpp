#include "stdafx.h"
#include "platform/PlatformUtils.h"

namespace NSPlatform
{
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

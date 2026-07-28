#include "stdafx.h"
#include "platform/PlatformUtils.h"

#include <unistd.h>

namespace NSPlatform
{
    std::filesystem::path GetExecutableDirectory()
    {
        // /proc/self/exe is a symlink to the running binary — the
        // standard way to do this on Linux (there's no portable
        // std::filesystem equivalent).
        std::error_code ec;
        std::filesystem::path exe = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (ec)
        {
            return std::filesystem::current_path();
        }

        return exe.parent_path();
    }
}

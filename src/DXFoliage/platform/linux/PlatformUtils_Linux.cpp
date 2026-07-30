#include "stdafx.h"
#include "platform/PlatformUtils.h"

#include "core/Defines.h"

#include <cstdlib>
#include <unistd.h>

namespace NSPlatform
{
    namespace
    {
        // XDG base directory spec: use the environment variable when it is
        // set AND absolute, otherwise the documented per-variable default.
        // The absolute check is in the spec - a relative value is defined
        // to be invalid and must be ignored rather than resolved.
        std::filesystem::path XdgDir(const char* envVar, const char* fallbackRelativeToHome)
        {
            if (const char* value = std::getenv(envVar); value and value[0] == '/')
            {
                return std::filesystem::path(value);
            }

            const char* home = std::getenv("HOME");
            if (not home or home[0] == '\0')
            {
                // No HOME at all (a daemon, or a stripped environment).
                // The working directory is a poor answer but a defined one.
                return std::filesystem::current_path();
            }

            return std::filesystem::path(home) / fallbackRelativeToHome;
        }

        std::filesystem::path AppSuffix()
        {
            // PROJECT_NAME is wide because the Windows APIs it was written
            // for are; the path here is narrow.
            const std::wstring_view wide = PROJECT_NAME;
            std::string narrow;
            narrow.reserve(wide.size());
            for (const wchar_t c : wide) narrow.push_back(static_cast<char>(c));
            return narrow;
        }
    }

    std::filesystem::path GetUserDataDirectory()
    {
        return XdgDir("XDG_DATA_HOME", ".local/share") / AppSuffix();
    }

    std::filesystem::path GetUserConfigDirectory()
    {
        return XdgDir("XDG_CONFIG_HOME", ".config") / AppSuffix();
    }

    std::filesystem::path GetUserCacheDirectory()
    {
        return XdgDir("XDG_CACHE_HOME", ".cache") / AppSuffix();
    }

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

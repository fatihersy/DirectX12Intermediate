#pragma once

#include "Tools.h"

enum class ELogLevel : uint8_t
{
    EFATAL = 0,
    EERROR,
    EWARN,
    EINFO,
    EDEBUG,
    ETRACE,
};

inline std::function<void(ELogLevel level, const std::string_view message)> g_PlatformConsoleWrite;

inline void g_FDebug(const std::string_view fmt)
{
    g_PlatformConsoleWrite(ELogLevel::EDEBUG, fmt);
}


inline void g_FError(const std::string_view fmt)
{
    g_PlatformConsoleWrite(ELogLevel::EERROR, fmt);
}

template<typename... Args>
inline void g_FDebug(const std::string_view fmt, Args&&... args)
{
    g_PlatformConsoleWrite(ELogLevel::EDEBUG, NSTool::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
inline void g_FError(const std::string_view fmt, Args&&... args)
{
    g_PlatformConsoleWrite(ELogLevel::EERROR, NSTool::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
inline void g_FWarn(const std::string_view fmt, Args&&... args)
{
    g_PlatformConsoleWrite(ELogLevel::EWARN, NSTool::format(fmt, std::forward<Args>(args)...));
}

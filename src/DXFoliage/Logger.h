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

// g_FDebug/g_FError already had this no-args overload; g_FWarn didn't.
// Without it a plain one-argument warning instantiates the variadic
// template, which hands the message to snprintf as a runtime format
// string — that's a -Wformat-security warning, and fatalwarnings "All"
// turns it into a build error.
inline void g_FWarn(const std::string_view fmt)
{
    g_PlatformConsoleWrite(ELogLevel::EWARN, fmt);
}

// EINFO/ETRACE existed in the enum but had no helpers until the Vulkan
// debug messenger started routing INFO and VERBOSE severities. Both
// overloads for the same -Wformat-security reason as g_FWarn above.
inline void g_FInfo(const std::string_view fmt)
{
    g_PlatformConsoleWrite(ELogLevel::EINFO, fmt);
}

inline void g_FTrace(const std::string_view fmt)
{
    g_PlatformConsoleWrite(ELogLevel::ETRACE, fmt);
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

template<typename... Args>
inline void g_FInfo(const std::string_view fmt, Args&&... args)
{
    g_PlatformConsoleWrite(ELogLevel::EINFO, NSTool::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
inline void g_FTrace(const std::string_view fmt, Args&&... args)
{
    g_PlatformConsoleWrite(ELogLevel::ETRACE, NSTool::format(fmt, std::forward<Args>(args)...));
}

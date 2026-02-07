#pragma once

#include <functional>
#include "Tool.h"

enum class FlogLevel : uint8_t {
    FLOG_FATAL = 0,
    FLOG_ERROR,
    FLOG_WARN,
    FLOG_INFO,
    FLOG_DEBUG,
    FLOG_TRACE
};

inline std::function<void(FlogLevel level, const std::string_view& message)> g_PlatformConsoleWrite;

template<typename... Args>
inline void g_FDebug(const char* fmt, Args&&... args) {
    g_PlatformConsoleWrite(FlogLevel::FLOG_DEBUG, FString::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
inline void g_FError(const char* fmt, Args&&... args) {
    g_PlatformConsoleWrite(FlogLevel::FLOG_ERROR, FString::format(fmt, std::forward<Args>(args)...));
}

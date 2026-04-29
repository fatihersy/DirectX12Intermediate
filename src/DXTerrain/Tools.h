#pragma once

namespace NSTool
{
    template<typename... Args>
    static std::string format(const std::string_view& format, Args&&... args)
    {
        auto to_str = []<typename T>(const T & v) -> decltype(auto)
        {
            if constexpr (std::is_same_v<T, std::string>) return v.c_str();
            else return v;
        };

        int size = std::snprintf(nullptr, 0, format.data(), to_str(args)...);
        if (size < 0) return {};
        std::vector<char> buf(static_cast<size_t>(size + 1));
        std::snprintf(buf.data(), buf.size(), format.data(), to_str(args)...);
        return std::string(buf.data(), size);
    }
    template<typename... Args>
    static std::wstring wformat(const std::wstring_view& format, Args&&... args)
    {
        auto to_wstr = []<typename T>(const T & v) -> decltype(auto)
        {
            if constexpr (std::is_same_v<T, std::wstring>) return v.c_str();
            else return v;
        };

        int size = std::swprintf(nullptr, 0, format.data(), to_wstr(args)...);
        if (size < 0) return {};
        std::vector<wchar_t> buf(static_cast<size_t>(size + 1));
        std::swprintf(buf.data(), buf.size(), format.data(), to_wstr(args)...);
        return std::wstring(buf.data(), size);
    }
};

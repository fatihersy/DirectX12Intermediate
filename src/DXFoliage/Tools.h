#pragma once

namespace NSTool
{
    // inline, NOT static. In a header, `static` gives every translation
    // unit its own private copy — so each TU that includes this without
    // calling format() carries a dead function, which is what
    // -Wunused-function was reporting 36 times (once per TU, one site).
    // `inline` is the correct keyword for a header definition: one shared
    // entity, no duplicates, no warning.

    // No-args overload. return the string directly without format
    inline std::string format(const std::string& str)
    {
        return str;
    }
    inline std::wstring wformat(const std::wstring& str)
    {
        return str;
    }

    template<typename... Args>
    static std::string format(const std::string_view format, Args&&... args)
    {
        auto to_str = []<typename T>(const T& v) -> decltype(auto)
        {
            using U = std::remove_cvref_t<T>;

            if constexpr (std::is_same_v<U, std::string>) return v.c_str();
            else if constexpr (std::is_same_v<U, std::string_view>) return v.data();
            else return v;
        };

        int size = std::snprintf(nullptr, 0, format.data(), to_str(args)...);
        if (size < 0) return {};

        std::vector<char> buf(static_cast<size_t>(size + 1));
        std::snprintf(buf.data(), buf.size(), format.data(), to_str(args)...);

        return std::string(buf.data(), size);
    }

    template<typename... Args>
    static std::wstring wformat(const std::wstring_view format, Args&&... args)
    {
        auto to_wstr = []<typename T>(const T& v) -> decltype(auto)
        {
            using U = std::remove_cvref_t<T>;

            if constexpr (std::is_same_v<U, std::wstring>) return v.c_str();
            else if constexpr (std::is_same_v<U, std::wstring_view>) return v.data();
            else return v;
        };

        int size = std::swprintf(nullptr, 0, format.data(), to_wstr(args)...);
        if (size < 0) return {};

        std::vector<wchar_t> buf(static_cast<size_t>(size + 1));
        std::swprintf(buf.data(), buf.size(), format.data(), to_wstr(args)...);

        return std::wstring(buf.data(), size);
    }

}

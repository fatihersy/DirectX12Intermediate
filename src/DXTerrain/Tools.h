#pragma once

class KTool
{
public:

    template<typename... Args>
    static std::string format(const std::string& format, Args&&... args)
    {
        int size = std::snprintf(nullptr, 0, format.c_str(), to_str(args)...);
        if (size < 0) return {};
        std::vector<char> buf(size + 1);
        std::snprintf(buf.data(), buf.size(), format.c_str(), to_str(args)...);
        return std::string(buf.data(), size);
    }
    template<typename... Args>
    static std::wstring wformat(const std::wstring& format, Args&&... args)
    {
        int size = std::swprintf(nullptr, 0, format.c_str(), to_wstr(args)...);
        if (size < 0) return {};
        std::vector<wchar_t> buf(size + 1);
        std::swprintf(buf.data(), buf.size(), format.c_str(), to_wstr(args)...);
        return std::wstring(buf.data(), size);
    }

private:

    static const char* to_str(const std::string& str)
    {
        return str.c_str();
    }

    static const wchar_t* to_wstr(const std::wstring& str)
    {
        return str.c_str();
    }

    template<typename T>
    static T to_str(const T& str)
    {
        return str;
    }
    template<typename T>
    static T to_wstr(const T& str)
    {
        return str;
    }
};

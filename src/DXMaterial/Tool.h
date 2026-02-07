#pragma once

#include <string>

class FString {
public:
    template<typename... Args>
    static inline std::string format(const std::string& format, Args&&... args)
    {
        size_t size = std::snprintf(nullptr, 0, format.c_str(), to_str(args)...) + 1;
        std::unique_ptr<char[]> buf(new char[size]);
        std::snprintf(buf.get(), size, format.c_str(), to_str(args)...);
        return std::string(buf.get(), buf.get() + size - 1);
    }

private:
    static const char* to_str(std::string&& t)
    {
        return t.c_str();
    }

    static const char* to_str(const std::string& t)
    {
        return t.c_str();
    }

    // universal reference here would be always selected, including std::string
    template<typename T>
    static T to_str(const T& t)
    {
        return t;
    }
};



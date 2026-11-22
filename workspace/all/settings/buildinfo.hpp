#pragma once

#include <istream>
#include <string>

struct BuildInfo
{
    std::string version;
    std::string revision;
};

inline BuildInfo readBuildInfo(std::istream &stamp)
{
    const auto readLine = [&stamp]() -> std::string {
        std::string value;
        std::getline(stamp, value);
        if (!value.empty() && value.back() == '\r')
            value.pop_back();
        return value.empty() ? "Unknown" : value;
    };
    return {readLine(), readLine()};
}

#pragma once

#include <chrono>
#include <sstream>
#include <string>

template<typename T>
std::string formatDuration(std::chrono::duration<T> duration)
{
    // the <format> STL seems to not be available on all used compilers yet
    auto h = std::chrono::duration_cast<std::chrono::hours>(duration);
    duration -= h;
    auto m = std::chrono::duration_cast<std::chrono::minutes>(duration);
    duration -= m;
    auto s = std::chrono::duration_cast<std::chrono::seconds>(duration);

    std::ostringstream oss;
    oss << h.count() << "h, " << m.count() << "m, " << s.count() << "s";
    return oss.str();
}

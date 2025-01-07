#pragma once

#include <iostream>
#include <print>

template <typename... Args>
void errorln(std::format_string<Args...> fmt, Args &&...args)
{
    std::println(std::cerr, fmt, std::forward<Args>(args)...);
}

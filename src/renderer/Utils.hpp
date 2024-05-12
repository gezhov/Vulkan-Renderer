#pragma once

#include "HeaderCore.hpp"

// std
#include <functional>
#include <string>

// from the answer on StackOverflow: https://stackoverflow.com/a/57595105
template <typename T, typename... Rest>
void hashCombine(std::size_t& seed, const T& v, const Rest&... rest)
{
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (hashCombine(seed, rest), ...);
}

VkResult createSemaphore(VkDevice device, VkSemaphore* outSemaphore);

std::string getTimeStampStr();

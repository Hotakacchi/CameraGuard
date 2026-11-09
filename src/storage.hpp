#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>

namespace guard {
constexpr std::uintmax_t MiB = 1024 * 1024;
constexpr std::uintmax_t reserveBytes = 256 * MiB;

// Returns false if insufficient free space remains after pruning.
bool prune(const std::filesystem::path &folder, std::chrono::hours age,
           std::uintmax_t limit, const std::filesystem::path &active = {},
           std::uintmax_t reserve = reserveBytes);
}

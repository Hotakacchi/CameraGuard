#include "storage.hpp"

#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace {
void require(bool passed)
{
    if (!passed)
        throw std::runtime_error("Recording retention test failed");
}
}

int main()
{
    const auto folder = fs::temp_directory_path()
        / ("camera-guard-test-" + std::to_string(std::random_device{}()));
    if (!fs::create_directory(folder))
        return 1;

    int result = 0;
    try {
        const auto now = fs::file_time_type::clock::now();
        const auto sample = [&](const char *name, std::chrono::hours age) {
            const auto path = folder / name;
            std::ofstream(path, std::ios::binary) << "12345678";
            fs::last_write_time(path, now - age);
            return path;
        };
        const auto expired = sample("guard_20260101_000000_000000000001.mp4", 200h);
        const auto oldest = sample("guard_20260101_000000_000000000002.mp4", 3h);
        const auto newer = sample("guard_20260101_000000_000000000003.mp4", 1h);
        const auto active = sample("guard_20260101_000000_000000000004.part.mp4", 200h);
        const auto orphan = sample("guard_20260101_000000_000000000005.part.mp4", 200h);
        const auto personal = sample("personal.mp4", 200h);
        require(guard::prune(folder, 168h, 16, active, 0));
        require(!fs::exists(expired) && !fs::exists(oldest) && !fs::exists(orphan));
        require(fs::exists(newer) && fs::exists(active) && fs::exists(personal));
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    fs::remove_all(folder);
    return result;
}

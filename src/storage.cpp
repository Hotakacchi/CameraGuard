#include "storage.hpp"

#include <algorithm>
#include <regex>
#include <vector>

namespace fs = std::filesystem;

bool guard::prune(const fs::path &folder, std::chrono::hours age, std::uintmax_t limit,
                  const fs::path &active, std::uintmax_t reserve)
{
    struct Clip {
        fs::path path;
        fs::file_time_type modified;
        std::uintmax_t size;
    };
#ifdef _WIN32
    static const std::wregex clipName(LR"(guard_[0-9]{8}_[0-9]{6}_[0-9a-f]{12}(\.part)?\.mp4)");
#else
    static const std::regex clipName(R"(guard_[0-9]{8}_[0-9]{6}_[0-9a-f]{12}(\.part)?\.mp4)");
#endif
    const auto cutoff = fs::file_time_type::clock::now() - age;
    std::uintmax_t used = 0;
    std::vector<Clip> clips;
    for (const auto &entry : fs::directory_iterator(folder)) {
        if (!fs::is_regular_file(entry.symlink_status())
            || !std::regex_match(entry.path().filename().native(), clipName))
            continue;

        const auto size = entry.file_size();
        used += size;
        if (entry.path().filename() != active.filename())
            clips.push_back({entry.path(), entry.last_write_time(), size});
    }

    std::sort(clips.begin(), clips.end(), [](const Clip &a, const Clip &b) {
        return a.modified == b.modified ? a.path < b.path : a.modified < b.modified;
    });
    auto free = fs::space(folder).available;
    std::error_code firstError;
    for (const auto &clip : clips) {
        if (clip.modified > cutoff && used <= limit && free >= reserve)
            break;
        std::error_code error;
        fs::remove(clip.path, error);
        if (error) {
            if (!firstError)
                firstError = error;
            continue;
        }
        used -= clip.size;
        free = fs::space(folder).available;
    }
    if (firstError)
        throw fs::filesystem_error("Cannot remove an old recording", folder, firstError);
    return free >= reserve;
}

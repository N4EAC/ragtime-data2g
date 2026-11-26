#include "ragtime/history/History.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#include "ragtime/files/SessionFiles.h"

#include "../LogLines.h"

namespace ragtime
{
namespace history
{

namespace
{

std::filesystem::path folderFor(const std::filesystem::path& file)
{
    std::filesystem::path name = file.filename();
    std::string stem = files::utf8FromPath(name.extension() == ".log" ? name.stem() : name);
    return file.parent_path() / files::pathFromUtf8(stem + "-history");
}

// The time on the file's first B line, or 0.
uint64_t firstBegan(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    std::string line;
    for (int i = 0; i < 64 && std::getline(in, line); i++)
    {
        if (line.compare(0, 2, "B\t") != 0) continue;
        std::vector<std::string> f = loglines::fields(line);
        uint64_t ms = 0;
        if (f.size() >= 2 && loglines::number(f[1], ms)) return ms;
    }
    return 0;
}

} // namespace

std::string historyFolder(const std::string& logPath)
{
    return files::utf8FromPath(folderFor(files::pathFromUtf8(logPath)));
}

std::string rolledName(uint64_t ms)
{
    std::time_t seconds = (std::time_t)(ms / 1000);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &seconds);
#else
    gmtime_r(&seconds, &utc);
#endif
    char name[96];
    std::snprintf(name, sizeof name, "%04d%02d%02d-%02d%02d%02d.log", utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                  utc.tm_hour, utc.tm_min, utc.tm_sec);
    return name;
}

bool rollLog(const std::string& logPath, uint64_t maxBytes, uint64_t nowMs)
{
    if (logPath.empty()) return false;
    std::filesystem::path file = files::pathFromUtf8(logPath);
    std::error_code ec;
    uintmax_t size = std::filesystem::file_size(file, ec);
    if (ec || size <= maxBytes) return false;
    uint64_t began = firstBegan(file);
    std::filesystem::path folder = folderFor(file);
    std::filesystem::create_directories(folder, ec);
    std::string name = rolledName(began != 0 ? began : nowMs);
    std::filesystem::path to = folder / name;
    // Two files can't start in the same second, but a clock set back can:
    // "~2" sorts after the first.
    for (int n = 2; std::filesystem::exists(to, ec); n++)
        to = folder / (name.substr(0, name.size() - 4) + "~" + std::to_string(n) + ".log");
    std::filesystem::rename(file, to, ec);
    return !ec;
}

std::vector<std::string> rolledLogs(const std::string& logPath)
{
    std::vector<std::string> out;
    if (logPath.empty()) return out;
    std::error_code ec;
    std::filesystem::path folder = folderFor(files::pathFromUtf8(logPath));
    if (!std::filesystem::is_directory(folder, ec)) return out;
    for (std::filesystem::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec))
    {
        if (!it->is_regular_file(ec) || it->path().extension() != ".log") continue;
        out.push_back(files::utf8FromPath(it->path()));
    }
    // Their names are their times: in name order, oldest first.
    std::sort(out.begin(), out.end());
    return out;
}

bool logExists(const std::string& path)
{
    std::error_code ec;
    return !path.empty() && std::filesystem::is_regular_file(files::pathFromUtf8(path), ec);
}

std::string readLog(const std::string& path)
{
    std::ifstream in(files::pathFromUtf8(path), std::ios::binary);
    if (!in) return std::string();
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

} // namespace history
} // namespace ragtime

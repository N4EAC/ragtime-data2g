#include "ragtime/search/HistorySearch.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

#include "ragtime/broadcast/BroadcastLog.h"
#include "ragtime/files/SessionFiles.h"
#include "ragtime/session/SessionLog.h"

namespace ragtime
{
namespace search
{

namespace
{

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

// One row of either log, as the search needs it.
struct Row
{
    bool rule = false;
    uint64_t atMs = 0;
    uint64_t beganMs = 0;       // a session rule's
    uint32_t time = 0;
    uint32_t seq = 0;
    std::string from;
    std::string to;
    std::string text;
    bool ours = false;
    bool file = false;
    bool picture = false;
    bool voice = false;
};

std::vector<Row> broadcastRows(const std::string& text)
{
    std::vector<Row> rows;
    for (broadcast::Message& m : broadcast::parseBroadcastLog(text, std::numeric_limits<size_t>::max()))
    {
        Row r;
        r.rule = m.kind == broadcast::Message::Kind::Rule;
        if (!r.rule && m.kind != broadcast::Message::Kind::Text && m.kind != broadcast::Message::Kind::File)
            continue;
        r.atMs = m.atMs;
        r.time = m.time;
        r.seq = m.seq;
        r.from = std::move(m.from);
        r.to = std::move(m.to);
        r.text = std::move(m.text);
        r.ours = m.ours;
        r.file = m.kind == broadcast::Message::Kind::File;
        r.picture = m.picture;
        r.voice = m.voice;
        rows.push_back(std::move(r));
    }
    return rows;
}

std::vector<Row> sessionRows(const std::string& text)
{
    std::vector<Row> rows;
    for (session::Message& m : session::parseSessionLog(text, std::numeric_limits<size_t>::max()))
    {
        Row r;
        r.rule = m.kind == session::Message::Kind::Rule;
        r.atMs = m.atMs;
        r.beganMs = m.beganMs;
        r.time = m.time;
        r.seq = m.seq;
        r.from = std::move(m.from);
        r.text = std::move(m.text);
        r.ours = m.ours;
        r.file = m.kind == session::Message::Kind::File;
        r.picture = m.picture;
        r.voice = m.voice;
        rows.push_back(std::move(r));
    }
    return rows;
}

bool sameCall(const std::string& a, const std::string& b)
{
    return foldCase(a) == foldCase(b);
}

int64_t modified(const std::filesystem::path& path)
{
    std::error_code ec;
    auto t = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return (int64_t)t.time_since_epoch().count();
}

// The rolled files in a history folder, newest first: they are named for
// the time their first run began (YYYYMMDD-HHMMSS.log), so by name.
std::vector<std::filesystem::path> rolled(const std::filesystem::path& folder)
{
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    if (!std::filesystem::is_directory(folder, ec)) return files;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec))
    {
        if (entry.path().extension() == ".log" && entry.is_regular_file(ec)) files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.filename() > b.filename(); });
    return files;
}

std::filesystem::path historyFolderOf(const std::filesystem::path& log)
{
    std::filesystem::path folder = log.parent_path();
    std::string stem = files::utf8FromPath(log.filename());
    if (stem.size() > 4 && stem.compare(stem.size() - 4, 4, ".log") == 0) stem.resize(stem.size() - 4);
    return folder / files::pathFromUtf8(stem + "-history");
}

} // namespace

std::string foldCase(const std::string& text)
{
    std::string out = text;
    for (char& c : out)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return out;
}

std::vector<std::string> searchTerms(const std::string& typed)
{
    std::vector<std::string> terms;
    std::string term;
    bool quoted = false;
    auto end = [&]() {
        if (!term.empty()) terms.push_back(foldCase(term));
        term.clear();
    };
    for (char c : typed)
    {
        if (c == '"')
        {
            end();
            quoted = !quoted;
        }
        else if (isSpace(c) && !quoted)
            end();
        else if (isSpace(c) && quoted)
        {
            // Runs of spaces in a phrase count as one.
            if (!term.empty() && term.back() != ' ') term += ' ';
        }
        else
            term += c;
    }
    end();
    for (std::string& t : terms)
        while (!t.empty() && t.back() == ' ') t.pop_back();
    terms.erase(std::remove(terms.begin(), terms.end(), std::string()), terms.end());
    return terms;
}

bool lineMatches(const std::vector<std::string>& terms, const std::string& text, const std::string& from,
                 const std::string& to)
{
    if (terms.empty()) return false;
    std::string hay = foldCase(text);
    hay += '\n';
    hay += foldCase(from);
    if (!to.empty())
    {
        hay += "\n@";
        hay += foldCase(to);
    }
    for (const std::string& t : terms)
        if (hay.find(t) == std::string::npos) return false;
    return true;
}

std::vector<std::pair<size_t, size_t>> matchSpans(const std::vector<std::string>& terms, const std::string& text)
{
    std::vector<std::pair<size_t, size_t>> spans;
    std::string hay = foldCase(text);
    for (const std::string& t : terms)
    {
        if (t.empty()) continue;
        for (size_t at = hay.find(t); at != std::string::npos; at = hay.find(t, at + t.size()))
            spans.push_back({at, at + t.size()});
    }
    std::sort(spans.begin(), spans.end());
    std::vector<std::pair<size_t, size_t>> merged;
    for (const auto& s : spans)
    {
        if (!merged.empty() && s.first <= merged.back().second)
            merged.back().second = std::max(merged.back().second, s.second);
        else
            merged.push_back(s);
    }
    return merged;
}

std::vector<Source> historySources(const std::string& broadcastLogPath, const std::string& sessionFolder)
{
    std::vector<Source> sources;
    std::error_code ec;
    if (!broadcastLogPath.empty())
    {
        std::filesystem::path log = files::pathFromUtf8(broadcastLogPath);
        if (std::filesystem::is_regular_file(log, ec)) sources.push_back({Place::Broadcast, "", broadcastLogPath});
        for (const auto& f : rolled(historyFolderOf(log)))
            sources.push_back({Place::Broadcast, "", files::utf8FromPath(f)});
    }
    if (!sessionFolder.empty())
    {
        std::filesystem::path folder = files::pathFromUtf8(sessionFolder);
        std::vector<std::pair<int64_t, std::filesystem::path>> logs;
        if (std::filesystem::is_directory(folder, ec))
        {
            for (const auto& entry : std::filesystem::directory_iterator(folder, ec))
            {
                const std::filesystem::path& p = entry.path();
                if (p.extension() != ".log" || !entry.is_regular_file(ec)) continue;
                logs.push_back({modified(p), p});
            }
        }
        // A station whose live log is gone but whose history is kept.
        if (std::filesystem::is_directory(folder, ec))
        {
            for (const auto& entry : std::filesystem::directory_iterator(folder, ec))
            {
                std::string name = files::utf8FromPath(entry.path().filename());
                const std::string tail = "-history";
                if (!entry.is_directory(ec) || name.size() <= tail.size()
                    || name.compare(name.size() - tail.size(), tail.size(), tail) != 0)
                    continue;
                std::filesystem::path live = folder / files::pathFromUtf8(name.substr(0, name.size() - tail.size()) + ".log");
                if (std::filesystem::exists(live, ec)) continue;
                std::vector<std::filesystem::path> files = rolled(entry.path());
                if (!files.empty()) logs.push_back({modified(files.front()), live});
            }
        }
        std::sort(logs.begin(), logs.end(), [](const auto& a, const auto& b) {
            return a.first != b.first ? a.first > b.first : a.second < b.second;
        });
        for (const auto& log : logs)
        {
            std::string name = files::utf8FromPath(log.second.filename());
            std::string peer = name.substr(0, name.size() - 4);
            if (std::filesystem::is_regular_file(log.second, ec))
                sources.push_back({Place::Ragchew, peer, files::utf8FromPath(log.second)});
            for (const auto& f : rolled(historyFolderOf(log.second)))
                sources.push_back({Place::Ragchew, peer, files::utf8FromPath(f)});
        }
    }
    return sources;
}

std::vector<Hit> searchText(const Source& source, const std::string& text, const Query& query)
{
    std::vector<Hit> hits;
    if (query.terms.empty()) return hits;
    if (source.place == Place::Broadcast ? !query.broadcast : !query.ragchews) return hits;
    const bool ragchew = source.place == Place::Ragchew;
    std::vector<Row> rows = ragchew ? sessionRows(text) : broadcastRows(text);

    // Each stretch between rules: a ragchew, or a run of Ragtime's for
    // Broadcast (nearby lines stay within it either way).
    size_t start = 0;
    for (size_t i = 0; i <= rows.size(); i++)
    {
        if (i < rows.size() && !rows[i].rule) continue;
        if (i > start)
        {
            uint64_t began = 0, ended = 0;
            std::string peer = source.peer;
            if (ragchew)
            {
                began = i < rows.size() && rows[i].beganMs ? rows[i].beganMs : rows[start].atMs;
                ended = i < rows.size() && rows[i].atMs ? rows[i].atMs : rows[i - 1].atMs;
                for (size_t k = start; k < i; k++)
                {
                    if (!rows[k].ours && !rows[k].file && !rows[k].from.empty())
                    {
                        peer = rows[k].from;
                        break;
                    }
                }
            }
            for (size_t k = start; k < i; k++)
            {
                const Row& r = rows[k];
                if (!query.from.empty() && !sameCall(query.from, r.from)) continue;
                if (query.sinceMs && r.atMs < query.sinceMs) continue;
                if (!lineMatches(query.terms, r.text, r.from, r.to)) continue;
                Hit h;
                h.place = source.place;
                h.peer = peer;
                h.beganMs = began;
                h.endedMs = ended;
                h.atMs = r.atMs;
                h.time = r.time;
                h.seq = r.seq;
                h.from = r.from;
                h.to = r.to;
                h.text = r.text;
                h.ours = r.ours;
                h.file = r.file;
                h.picture = r.picture;
                h.voice = r.voice;
                size_t first = k - std::min(k - start, NEARBY_LINES);
                size_t last = std::min(i, k + NEARBY_LINES + 1);
                for (size_t n = first; n < last; n++)
                    h.nearby.push_back({rows[n].atMs, rows[n].from, rows[n].to, rows[n].text, rows[n].ours, rows[n].file});
                h.at = k - first;
                hits.push_back(std::move(h));
            }
        }
        start = i + 1;
    }
    std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.atMs > b.atMs; });
    return hits;
}

std::vector<Hit> searchFile(const Source& source, const Query& query)
{
    std::ifstream in(files::pathFromUtf8(source.path), std::ios::binary);
    if (!in) return {};
    std::ostringstream text;
    text << in.rdbuf();
    return searchText(source, text.str(), query);
}

bool samePlace(const Hit& a, const Hit& b)
{
    if (a.place != b.place) return false;
    if (a.place == Place::Broadcast) return true;
    return sameCall(a.peer, b.peer) && a.beganMs == b.beganMs;
}

void sortHits(std::vector<Hit>& hits)
{
    std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
        if (a.place != b.place) return a.place == Place::Broadcast;
        if (a.place == Place::Ragchew && !samePlace(a, b))
            return std::make_tuple(a.beganMs, foldCase(a.peer)) > std::make_tuple(b.beganMs, foldCase(b.peer));
        return a.atMs > b.atMs;
    });
}

} // namespace search
} // namespace ragtime

// The chat logs kept for good, and read a page at a time.
//
// A log (broadcast::BroadcastLog's file, or a station's session::SessionLog
// file) that grows past its size at the start of a run is rolled: moved
// whole into a history folder beside it, named for when its first run
// began, and a new file is started. Nothing is ever cut or deleted (Jeff,
// 2026-11-25: keep it all). For a log "broadcast.log" the folder is
// "broadcast-history", holding "20261103-041200.log" and so on; every
// rolled file holds whole runs, from a B line to its E, so each reads on
// its own.
//
// Pages reads it back from the newest end: the first call gives the newest
// rows, each later one the rows before those, a file at a time as they are
// needed, so a tab opens with a page however long the history has grown.
// Rows handed back (a tab dropping what it holds past its cap) come again
// at the next call.
//
// A reaction logged after its line's file was rolled away isn't shown, as
// before when the file was cut.
#ifndef RAGTIME_HISTORY_HISTORY_H
#define RAGTIME_HISTORY_HISTORY_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace ragtime
{
namespace history
{

// The folder a log's rolled files go to: "W1AW.log" -> "W1AW-history",
// beside it.
std::string historyFolder(const std::string& logPath);

// The name a rolled file gets for the run that starts it, at ms (UTC):
// "20261103-041200.log".
std::string rolledName(uint64_t ms);

// Moves the log into its history folder if it is bigger than maxBytes,
// named for the time on its first B line (or nowMs without one). False if
// nothing moved.
bool rollLog(const std::string& logPath, uint64_t maxBytes, uint64_t nowMs);

// The log's rolled files, oldest first, as full paths.
std::vector<std::string> rolledLogs(const std::string& logPath);

// The whole file as it is, or "" when it can't be read.
std::string readLog(const std::string& path);
// True if there is a file there.
bool logExists(const std::string& path);

template <class Row>
class Pages
{
public:
    // Every row of one file's text, oldest first.
    using Parse = std::function<std::vector<Row>(const std::string& text)>;

    Pages() = default;
    // The log's newest file and its rolled ones, oldest last. A Rule row
    // (isRule) is never the first row of the history.
    Pages(const std::string& logPath, Parse parse, std::function<bool(const Row&)> isRule)
        : parse_(std::move(parse))
        , isRule_(std::move(isRule))
    {
        if (logPath.empty()) return;
        if (logExists(logPath)) files_.push_back(logPath);
        std::vector<std::string> rolled = rolledLogs(logPath);
        for (auto it = rolled.rbegin(); it != rolled.rend(); ++it) files_.push_back(*it);
    }

    // Up to max rows from before those given so far, oldest first: the
    // newest at the first call. Empty when the history has no more.
    std::vector<Row> older(size_t max)
    {
        while (above_.size() < max && next_ < files_.size())
        {
            std::vector<Row> rows = parse_ ? parse_(readLog(files_[next_])) : std::vector<Row>();
            next_++;
            above_.insert(above_.begin(), std::make_move_iterator(rows.begin()), std::make_move_iterator(rows.end()));
        }
        // The history starts with a row, not a rule.
        if (next_ == files_.size())
            while (!above_.empty() && isRule_ && isRule_(above_.front())) above_.pop_front();
        size_t take = std::min(max, above_.size());
        std::vector<Row> page(std::make_move_iterator(above_.end() - (std::ptrdiff_t)take),
                              std::make_move_iterator(above_.end()));
        above_.erase(above_.end() - (std::ptrdiff_t)take, above_.end());
        return page;
    }

    // Rows given before, oldest first, put back below what is left: the
    // next call starts with them.
    void giveBack(std::vector<Row> rows)
    {
        above_.insert(above_.end(), std::make_move_iterator(rows.begin()), std::make_move_iterator(rows.end()));
    }

    // True while older() has rows to give.
    bool more() const
    {
        if (next_ < files_.size()) return true;
        for (const Row& row : above_)
            if (!isRule_ || !isRule_(row)) return true;
        return false;
    }

private:
    Parse parse_;
    std::function<bool(const Row&)> isRule_;
    std::vector<std::string> files_;    // newest first
    size_t next_ = 0;                   // the next file to read
    std::deque<Row> above_;             // read and not given, oldest first
};

} // namespace history
} // namespace ragtime

#endif

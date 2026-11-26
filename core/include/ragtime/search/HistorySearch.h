// Searching what was said: the Broadcast log and every station's session
// log, with their rolled history, for the Search tab (Ctrl+F, Jeff
// 2026-11-25).
//
// The search reads the log files, not the rows the tabs have loaded, so it
// finds a line however much of History the tabs hold. It works one file at
// a time (searchFile), so the app can run it on a thread of its own and
// show hits as they come, newest files first (historySources).
//
// A line matches when every term is in it, in any order, ignoring the case
// of ASCII letters: in its text (a file line's name), its sender's
// callsign, or the station it is addressed to ("@W1AW"). A term is a word,
// or several in double quotes. No socket, no wx; not thread safe, but
// nothing here keeps state between calls.
#ifndef RAGTIME_SEARCH_HISTORY_SEARCH_H
#define RAGTIME_SEARCH_HISTORY_SEARCH_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ragtime
{
namespace search
{

// What was typed, cut into terms, lower-cased (ASCII only): words, and
// "phrases in quotes" as one term each. Empty terms are dropped.
std::vector<std::string> searchTerms(const std::string& typed);

// ASCII letters lower-cased; other bytes as they are.
std::string foldCase(const std::string& text);

struct Query
{
    std::vector<std::string> terms;     // from searchTerms
    std::string from;                   // only this sender's lines, or "" for anyone
    uint64_t sinceMs = 0;               // only lines from then on, or 0 for any time
    bool broadcast = true;              // look in the Broadcast log
    bool ragchews = true;               // look in the session logs
};

// Whether a line with this text, sender and addressee has every term.
bool lineMatches(const std::vector<std::string>& terms, const std::string& text, const std::string& from,
                 const std::string& to = std::string());

// Where the terms are in text, as byte ranges [first, second), in order and
// merged where they overlap, for marking them.
std::vector<std::pair<size_t, size_t>> matchSpans(const std::vector<std::string>& terms, const std::string& text);

enum class Place
{
    Broadcast,
    Ragchew,
};

// A line next to a hit, for showing the hit in place.
struct Nearby
{
    uint64_t atMs = 0;
    std::string from;
    std::string to;
    std::string text;
    bool ours = false;
    bool file = false;
};

struct Hit
{
    Place place = Place::Broadcast;
    std::string peer;           // a ragchew's station
    // The ragchew the line was in: when it began and ended (the first and
    // last lines' times when the log doesn't say). Broadcast: 0.
    uint64_t beganMs = 0;
    uint64_t endedMs = 0;

    // The line itself, and what finds it again in a loaded chat: when it
    // was made or heard, its sender and its record's number.
    uint64_t atMs = 0;
    uint32_t time = 0;
    uint32_t seq = 0;
    std::string from;
    std::string to;
    std::string text;           // a file line: its name, or the log's words for it
    bool ours = false;
    bool file = false;
    bool picture = false;
    bool voice = false;

    // A few lines either side, in the same ragchew (or the same Broadcast
    // file), oldest first, with the hit at index `at`.
    std::vector<Nearby> nearby;
    size_t at = 0;
};

// A log file to search.
struct Source
{
    Place place = Place::Broadcast;
    std::string peer;           // a session log's station, as its file names it
    std::string path;           // UTF-8
};

// The files to search, newest first: the Broadcast log and its rolled
// history ("broadcast.log" and "broadcast-history/*.log"), then each
// station's session log and its history ("W1AW.log" and
// "W1AW-history/*.log" in the sessions folder), the most recently written
// first. Missing ones are left out; "" leaves out that half.
std::vector<Source> historySources(const std::string& broadcastLogPath, const std::string& sessionFolder);

// Lines either side of a hit kept in Hit::nearby.
constexpr size_t NEARBY_LINES = 3;

// The hits in one log's text, newest first. A session log's station is
// the source's peer.
std::vector<Hit> searchText(const Source& source, const std::string& text, const Query& query);
// The same, reading the file (nothing if it can't be read).
std::vector<Hit> searchFile(const Source& source, const Query& query);

// Hits in the order the tab lists them: the Broadcast ones first, then
// each ragchew, the latest first; newest first within each.
void sortHits(std::vector<Hit>& hits);

// Whether two hits are in the same place: Broadcast, or the same ragchew.
bool samePlace(const Hit& a, const Hit& b);

} // namespace search
} // namespace ragtime

#endif

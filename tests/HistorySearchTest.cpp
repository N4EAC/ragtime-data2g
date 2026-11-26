// The Search tab's core: terms and phrases, matching text, callsigns and
// addressees, the marks, Broadcast and session logs written by their own
// classes, From and since, hits grouped by ragchew with lines nearby, the
// files found newest first with their rolled history, and the tab's order.
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "Check.h"
#include "ragtime/broadcast/BroadcastChat.h"
#include "ragtime/broadcast/BroadcastLog.h"
#include "ragtime/search/HistorySearch.h"
#include "ragtime/session/SessionChat.h"
#include "ragtime/session/SessionLog.h"

using namespace ragtime;
using search::Hit;
using search::Place;
using search::Query;
using search::Source;

namespace
{

const uint64_t T0 = 1795564800000; // 2026-11-25 00:00 UTC

std::filesystem::path scratch(const std::string& name)
{
    std::filesystem::path dir = std::filesystem::temp_directory_path() / ("ragtime-" + name + "-test");
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

broadcast::Message bline(uint64_t id, bool ours, const std::string& from, const std::string& to,
                         const std::string& text, uint64_t atMs)
{
    broadcast::Message m;
    m.id = id;
    m.ours = ours;
    m.from = from;
    m.to = to;
    m.text = text;
    m.atMs = atMs;
    m.time = (uint32_t)(atMs / 1000);
    m.seq = (uint32_t)id;
    m.state = ours ? broadcast::Message::State::Sent : broadcast::Message::State::Heard;
    return m;
}

session::Message sline(uint64_t id, bool ours, const std::string& from, const std::string& text, uint64_t atMs)
{
    session::Message m;
    m.id = id;
    m.ours = ours;
    m.from = from;
    m.text = text;
    m.atMs = atMs;
    m.time = (uint32_t)(atMs / 1000);
    m.seq = (uint32_t)id;
    m.state = ours ? session::Message::State::Delivered : session::Message::State::Heard;
    return m;
}

Query query(const std::string& typed)
{
    Query q;
    q.terms = search::searchTerms(typed);
    return q;
}

void terms()
{
    auto t = search::searchTerms("  Dipole  \"Ladder   LINE\" w1aw ");
    CHECK_EQ(t.size(), (size_t)3);
    CHECK_EQ(t[0], std::string("dipole"));
    CHECK_EQ(t[1], std::string("ladder line"));
    CHECK_EQ(t[2], std::string("w1aw"));
    CHECK(search::searchTerms("").empty());
    CHECK(search::searchTerms("\"\"  ").empty());
    // An unclosed quote runs to the end.
    t = search::searchTerms("\"end fed");
    CHECK_EQ(t.size(), (size_t)1);
    CHECK_EQ(t[0], std::string("end fed"));
    // Only ASCII is folded: other bytes stay as they are.
    CHECK_EQ(search::foldCase("Ünd DX"), std::string("Ünd dx"));
}

void matching()
{
    auto t = search::searchTerms("dipole w1aw");
    CHECK(search::lineMatches(t, "New DIPOLE is up", "W1AW"));
    CHECK(!search::lineMatches(t, "New dipole is up", "KC2G"));
    // The addressee counts as "@CALL".
    CHECK(search::lineMatches(search::searchTerms("@vk2abc path"), "how is the path?", "AG7EW", "VK2ABC"));
    CHECK(!search::lineMatches(search::searchTerms("@vk2abc"), "how is the path?", "AG7EW"));
    // A term can't straddle the text and the callsign.
    CHECK(!search::lineMatches(search::searchTerms("upw1aw"), "is up", "W1AW"));
    CHECK(!search::lineMatches({}, "anything", "W1AW"));

    auto spans = search::matchSpans(search::searchTerms("pole dipole up"), "Dipole up, dipole");
    CHECK_EQ(spans.size(), (size_t)3);
    CHECK(spans[0] == std::make_pair((size_t)0, (size_t)6));
    CHECK(spans[1] == std::make_pair((size_t)7, (size_t)9));
    CHECK(spans[2] == std::make_pair((size_t)11, (size_t)17));
}

void broadcastLog()
{
    std::filesystem::path dir = scratch("search-broadcast");
    std::string path = (dir / "broadcast.log").string();
    broadcast::BroadcastLog log(path);
    log.began(T0);
    log.added(bline(1, false, "KC2G", "", "CQ D2G de KC2G", T0 + 1000));
    log.added(bline(2, true, "AG7EW", "KC2G", "QSL, the new dipole is up", T0 + 2000));
    log.added(bline(3, false, "KC2G", "AG7EW", "FB, how high is the dipole?", T0 + 3000));
    log.added(bline(4, true, "AG7EW", "", "About 10 m", T0 + 4000));
    log.added(bline(5, false, "G4XYZ", "", "73", T0 + 5000));
    log.ended(T0 + 6000);

    Source src{Place::Broadcast, "", path};
    std::vector<Hit> hits = search::searchFile(src, query("Dipole"));
    CHECK_EQ(hits.size(), (size_t)2);
    // Newest first, with what finds it again and the lines either side.
    CHECK_EQ(hits[0].from, std::string("KC2G"));
    CHECK_EQ(hits[0].atMs, T0 + 3000);
    CHECK_EQ(hits[0].seq, (uint32_t)3);
    CHECK_EQ(hits[0].to, std::string("AG7EW"));
    CHECK_EQ(hits[0].nearby.size(), (size_t)5);
    CHECK_EQ(hits[0].at, (size_t)2);
    CHECK_EQ(hits[0].nearby[hits[0].at].text, std::string("FB, how high is the dipole?"));
    CHECK_EQ(hits[1].nearby.size(), (size_t)5);
    CHECK_EQ(hits[1].at, (size_t)1);
    CHECK(hits[1].ours);

    // From, since, and Broadcast left out.
    Query q = query("dipole");
    q.from = "ag7ew";
    CHECK_EQ(search::searchFile(src, q).size(), (size_t)1);
    q = query("dipole");
    q.sinceMs = T0 + 2500;
    CHECK_EQ(search::searchFile(src, q).size(), (size_t)1);
    q = query("dipole");
    q.broadcast = false;
    CHECK(search::searchFile(src, q).empty());
    // A callsign alone finds its lines and the ones addressed to it.
    CHECK_EQ(search::searchFile(src, query("kc2g")).size(), (size_t)3);
    // Nothing typed finds nothing.
    CHECK(search::searchFile(src, query("")).empty());
}

void sessionLog()
{
    std::filesystem::path dir = scratch("search-sessions");
    session::SessionLog log(dir.string());
    log.began("W1AW", T0);
    log.added("W1AW", sline(1, false, "W1AW", "Hello, how copy?", T0 + 1000));
    log.added("W1AW", sline(2, true, "AG7EW", "Solid copy, rig into a dipole", T0 + 2000));
    log.ended("W1AW", T0 + 9000);
    log.began("W1AW", T0 + 100000);
    log.added("W1AW", sline(1, true, "AG7EW", "Dipole is down for the storm", T0 + 101000));
    session::Message note;
    note.kind = session::Message::Kind::File;
    note.ours = true;
    note.from = "AG7EW";
    note.text = "dipole-centre.j2k to W1AW: sent";
    note.atMs = T0 + 102000;
    note.picture = true;
    note.path = "/home/op/dipole-centre.j2k";
    log.file("W1AW", note);
    // The app stopped under this one: no E.

    Source src{Place::Ragchew, "W1AW", log.pathFor("W1AW")};
    std::vector<Hit> hits = search::searchFile(src, query("dipole"));
    CHECK_EQ(hits.size(), (size_t)3);
    // Each ragchew's times: the log's B and E, or the last line's.
    CHECK_EQ(hits[0].beganMs, T0 + 100000);
    CHECK_EQ(hits[0].endedMs, T0 + 102000);
    CHECK(hits[0].file);
    CHECK(hits[0].picture);
    CHECK_EQ(hits[2].beganMs, T0);
    CHECK_EQ(hits[2].endedMs, T0 + 9000);
    CHECK_EQ(hits[2].peer, std::string("W1AW"));
    // Nearby lines stay inside their ragchew.
    CHECK_EQ(hits[2].nearby.size(), (size_t)2);
    CHECK_EQ(hits[1].nearby.size(), (size_t)2);
    CHECK(search::samePlace(hits[0], hits[1]));
    CHECK(!search::samePlace(hits[1], hits[2]));
    Query q = query("dipole");
    q.ragchews = false;
    CHECK(search::searchFile(src, q).empty());
}

void sourcesAndOrder()
{
    std::filesystem::path dir = scratch("search-sources");
    std::error_code ec;
    std::filesystem::create_directories(dir / "broadcast-history", ec);
    std::filesystem::create_directories(dir / "sessions" / "W1AW-history", ec);
    std::filesystem::create_directories(dir / "sessions" / "OLD1-history", ec);
    auto touch = [](const std::filesystem::path& p) { std::ofstream(p) << "x\n"; };
    touch(dir / "broadcast.log");
    touch(dir / "broadcast-history" / "20261101-120000.log");
    touch(dir / "broadcast-history" / "20261115-080000.log");
    touch(dir / "broadcast-history" / "notes.txt");
    touch(dir / "sessions" / "KC2G.log");
    touch(dir / "sessions" / "W1AW.log");
    touch(dir / "sessions" / "W1AW-history" / "20261118-201400.log");
    touch(dir / "sessions" / "OLD1-history" / "20260101-000000.log");
    auto now = std::filesystem::file_time_type::clock::now();
    std::filesystem::last_write_time(dir / "sessions" / "KC2G.log", now - std::chrono::hours(48), ec);
    std::filesystem::last_write_time(dir / "sessions" / "W1AW.log", now - std::chrono::hours(1), ec);
    std::filesystem::last_write_time(dir / "sessions" / "OLD1-history" / "20260101-000000.log",
                                     now - std::chrono::hours(2000), ec);

    std::vector<Source> s = search::historySources((dir / "broadcast.log").string(), (dir / "sessions").string());
    CHECK_EQ(s.size(), (size_t)7);
    if (s.size() == 7)
    {
        CHECK(s[0].place == Place::Broadcast);
        CHECK(s[0].path.find("broadcast.log") != std::string::npos);
        CHECK(s[1].path.find("20261115") != std::string::npos);
        CHECK(s[2].path.find("20261101") != std::string::npos);
        CHECK_EQ(s[3].peer, std::string("W1AW"));
        CHECK(s[3].path.find("W1AW.log") != std::string::npos);
        CHECK(s[4].path.find("20261118") != std::string::npos);
        CHECK_EQ(s[5].peer, std::string("KC2G"));
        CHECK_EQ(s[6].peer, std::string("OLD1"));
        CHECK(s[6].place == Place::Ragchew);
    }
    CHECK(search::historySources("", "").empty());
    CHECK(search::historySources((dir / "nothing.log").string(), (dir / "none").string()).empty());

    // The tab's order: Broadcast first, then each ragchew, the latest first.
    std::vector<Hit> hits(4);
    hits[0].place = Place::Ragchew, hits[0].peer = "W1AW", hits[0].beganMs = 10, hits[0].atMs = 11;
    hits[1].place = Place::Broadcast, hits[1].atMs = 5;
    hits[2].place = Place::Ragchew, hits[2].peer = "KC2G", hits[2].beganMs = 20, hits[2].atMs = 21;
    hits[3].place = Place::Broadcast, hits[3].atMs = 50;
    search::sortHits(hits);
    CHECK(hits[0].place == Place::Broadcast && hits[0].atMs == 50);
    CHECK(hits[1].place == Place::Broadcast && hits[1].atMs == 5);
    CHECK_EQ(hits[2].peer, std::string("KC2G"));
    CHECK_EQ(hits[3].peer, std::string("W1AW"));
}

} // namespace

int main()
{
    terms();
    matching();
    broadcastLog();
    sessionLog();
    sourcesAndOrder();
    return ragtime_test::finish();
}

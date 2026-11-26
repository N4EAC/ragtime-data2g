// BroadcastLog, the Broadcast tab's lasting log: its lines read back as
// rows with a rule after each run of Ragtime, text with tabs and newlines
// in it, a run cut short, file lines and how they ended, the newest rows
// only, lines it doesn't know, and the file rolled into its history folder
// once it grows too big, read back a page at a time.
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "Check.h"
#include "ragtime/broadcast/BroadcastChat.h"
#include "ragtime/broadcast/BroadcastLog.h"
#include "ragtime/history/History.h"

using namespace ragtime;
using broadcast::BroadcastLog;
using broadcast::Message;
using Kind = Message::Kind;
using MState = Message::State;

namespace
{

const uint64_t T0 = 1792627200000; // 2026-10-22 00:00 UTC

Message text(uint64_t id, bool ours, const std::string& from, const std::string& t, uint64_t atMs,
             MState state = MState::Heard)
{
    Message m;
    m.id = id;
    m.ours = ours;
    m.from = from;
    m.text = t;
    m.atMs = atMs;
    m.time = (uint32_t)(atMs / 1000);
    m.seq = (uint32_t)id;
    m.state = state;
    return m;
}

Message fileCard(uint64_t id, bool ours, const std::string& from, const std::string& name, uint64_t atMs)
{
    Message m;
    m.id = id;
    m.kind = Kind::File;
    m.file = 100 + id;
    m.ours = ours;
    m.from = from;
    m.text = name;
    m.atMs = atMs;
    return m;
}

std::filesystem::path scratch(const std::string& name)
{
    std::filesystem::path dir = std::filesystem::temp_directory_path() / ("ragtime-" + name + "-test");
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    return dir;
}

std::string readAll(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

//-------------------------------------------------------------------------

void testTwoRunsAndTheirRules()
{
    std::filesystem::path dir = scratch("broadcast-log");
    std::string path = (dir / "broadcast.log").string();
    BroadcastLog log(path);
    CHECK(log.load(100).empty());   // nothing yet, and no folder made
    CHECK(!std::filesystem::exists(dir));

    // The first run: a CQ of ours, an answer, an addressed one acknowledged,
    // a file heard and saved, and one of ours still going when it closed.
    log.began(T0);
    Message cq = text(1, true, "AG7EW", "CQ CQ de AG7EW", T0 + 1000, MState::Queued);
    cq.mode = "qpsk-r1/3";
    log.added(cq);
    cq.state = MState::Sent;
    cq.mode = "fsk16r25-r1/3";
    log.settled(cq);
    log.added(text(2, false, "W1AW", "QST de W1AW:\tthe net\nat 0100Z, C:\\net", T0 + 5000));
    Message addressed = text(3, true, "AG7EW", "gm Hiram", T0 + 9000, MState::Queued);
    addressed.to = "W1AW";
    addressed.ackWanted = true;
    log.added(addressed);
    addressed.state = MState::Acknowledged;
    log.settled(addressed);
    Message card = fileCard(4, false, "W1AW", "bulletin.txt", T0 + 12000);
    log.added(card);
    log.fileEnded(card, "Saved");
    log.added(text(5, true, "AG7EW", "still going", T0 + 13000, MState::Sending));
    log.ended(T0 + 60 * 60 * 1000);

    // The second run, cut short: no E.
    log.began(T0 + 2 * 60 * 60 * 1000);
    log.added(text(1, false, "KC2G", "QRV", T0 + 2 * 60 * 60 * 1000 + 5000));
    log.added(fileCard(2, true, "AG7EW", "map.png", T0 + 2 * 60 * 60 * 1000 + 6000));

    std::vector<Message> rows = log.load(100);
    CHECK_EQ(rows.size(), (size_t)9);
    if (rows.size() == 9)
    {
        CHECK(rows[0].past && rows[0].ours && rows[0].state == MState::Sent);
        CHECK_EQ(rows[0].mode, std::string("fsk16r25-r1/3"));
        CHECK_EQ(rows[0].seq, (uint32_t)1);
        CHECK_EQ(rows[0].time, (uint32_t)((T0 + 1000) / 1000));
        CHECK_EQ(rows[1].text, std::string("QST de W1AW:\tthe net\nat 0100Z, C:\\net"));
        CHECK(rows[1].state == MState::Heard && !rows[1].ours);
        CHECK(rows[2].state == MState::Acknowledged && rows[2].ackWanted);
        CHECK_EQ(rows[2].to, std::string("W1AW"));
        CHECK(rows[3].kind == Kind::File && rows[3].file == 0);
        CHECK_EQ(rows[3].text, std::string("bulletin.txt"));
        CHECK_EQ(rows[3].outcome, std::string("Saved"));
        CHECK(rows[4].state == MState::NotSent);    // still Sending when it closed
        CHECK(rows[5].kind == Kind::Rule);
        CHECK_EQ(rows[5].atMs, T0 + 60 * 60 * 1000);
        CHECK_EQ(rows[6].from, std::string("KC2G"));
        CHECK(rows[7].kind == Kind::File && rows[7].ours && rows[7].outcome.empty());
        // The run cut short ends at its last row.
        CHECK(rows[8].kind == Kind::Rule);
        CHECK_EQ(rows[8].atMs, T0 + 2 * 60 * 60 * 1000 + 6000);
    }

    // The newest rows only, and never a rule on top.
    rows = log.load(4);
    CHECK_EQ(rows.size(), (size_t)3);
    if (!rows.empty()) CHECK_EQ(rows.front().from, std::string("KC2G"));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

void testOddLines()
{
    // Lines it doesn't know, broken numbers, a state for a row it never
    // saw, and runs with no rows draw no rule.
    std::string text = "ragtime broadcast log 1\n"
                       "B\t1000\n"
                       "E\t2000\n"
                       "B\t3000\n"
                       "X\twhatever comes later\n"
                       "M\t1\t3500\t3\t7\t0\tH\tW1AW\t\t0\t\thello\r\n"
                       "M\tnope\t3500\t3\t7\t0\tH\tW1AW\t\t0\t\tbroken\n"
                       "S\t9\tS\t\n"
                       "R\t1\tSaved\n"
                       "E\t4000\n"
                       "B\t5000\n";
    std::vector<Message> rows = broadcast::parseBroadcastLog(text, 100);
    CHECK_EQ(rows.size(), (size_t)2);
    if (rows.size() == 2)
    {
        CHECK_EQ(rows[0].text, std::string("hello"));
        CHECK(rows[0].outcome.empty());             // R names files only
        CHECK(rows[1].kind == Kind::Rule && rows[1].atMs == 4000);
    }
    CHECK(broadcast::parseBroadcastLog("", 100).empty());
}

// Every row of the history, read a page at a time from the newest, put
// back in order.
std::vector<Message> allByPages(const BroadcastLog& log, size_t page, size_t& pages)
{
    history::Pages<Message> history = log.history();
    std::vector<Message> all;
    pages = 0;
    while (history.more())
    {
        std::vector<Message> rows = history.older(page);
        if (rows.empty()) break;
        pages++;
        all.insert(all.begin(), rows.begin(), rows.end());
    }
    CHECK(history.older(page).empty());
    return all;
}

void testRolledAndPaged()
{
    std::filesystem::path dir = scratch("broadcast-log-roll");
    std::string path = (dir / "broadcast.log").string();
    BroadcastLog log(path);
    std::string filler(200, 'x');

    // Many runs, past the size: the next run's start moves the file whole
    // into the history folder, named for its first run, and starts anew.
    uint64_t t = T0;
    uint64_t id = 1;
    size_t runs = 0;
    while (std::filesystem::exists(path) == false || std::filesystem::file_size(path) <= BroadcastLog::MAX_FILE_BYTES)
    {
        log.began(t);
        for (int i = 0; i < 50; i++) log.added(text(id++, false, "W1AW", filler, t += 1000));
        log.ended(t += 1000);
        runs++;
    }
    const size_t rowsBefore = log.load(1000000).size();
    CHECK_EQ(rowsBefore, runs * 51);
    log.began(t += 1000);
    CHECK_EQ(readAll(path).compare(0, 26, "ragtime broadcast log 1\nB\t"), 0);
    CHECK(std::filesystem::file_size(path) < 100);
    const std::string folder = history::historyFolder(path);
    CHECK_EQ(std::filesystem::path(folder).filename().string(), std::string("broadcast-history"));
    std::vector<std::string> rolled = history::rolledLogs(path);
    CHECK_EQ(rolled.size(), (size_t)1);
    if (rolled.size() == 1)
        CHECK_EQ(std::filesystem::path(rolled[0]).filename().string(), history::rolledName(T0));
    CHECK_EQ(history::rolledName(T0), std::string("20261022-000000.log"));

    // Nothing lost: every row still reads, the newest first.
    log.added(text(id++, false, "KC2G", "after the roll", t += 1000));
    log.ended(t += 1000);
    std::vector<Message> rows = log.load(1000000);
    CHECK_EQ(rows.size(), rowsBefore + 2);
    if (!rows.empty())
    {
        CHECK_EQ(rows.front().text, filler);
        CHECK(rows.back().kind == Kind::Rule);
        CHECK_EQ(rows[rows.size() - 2].text, std::string("after the roll"));
    }
    // The newest page alone.
    std::vector<Message> newest = log.load(10);
    CHECK_EQ(newest.size(), (size_t)10);
    if (newest.size() == 10) CHECK_EQ(newest[8].text, std::string("after the roll"));

    // A page at a time, through both files, gives the same rows in order.
    size_t pages = 0;
    std::vector<Message> paged = allByPages(log, 500, pages);
    CHECK_EQ(paged.size(), rows.size());
    CHECK_EQ(pages, (rows.size() + 499) / 500);
    bool same = paged.size() == rows.size();
    for (size_t i = 0; same && i < rows.size(); i++)
        same = paged[i].kind == rows[i].kind && paged[i].text == rows[i].text && paged[i].atMs == rows[i].atMs;
    CHECK(same);

    // A second roll: files read newest first, a rule between them.
    while (std::filesystem::file_size(path) <= BroadcastLog::MAX_FILE_BYTES)
    {
        log.began(t);
        for (int i = 0; i < 50; i++) log.added(text(id++, false, "W1AW", filler, t += 1000));
        log.ended(t += 1000);
    }
    const uint64_t secondRun = t;
    log.began(t += 1000);
    rolled = history::rolledLogs(path);
    CHECK_EQ(rolled.size(), (size_t)2);
    paged = allByPages(log, 333, pages);
    rows = log.load(1000000);
    CHECK_EQ(paged.size(), rows.size());
    CHECK(!paged.empty() && paged.front().kind != Kind::Rule);
    (void)secondRun;

    // Rows given back come again at the next page.
    history::Pages<Message> h = log.history();
    std::vector<Message> first = h.older(5);
    std::vector<Message> back(first.begin() + 3, first.end());
    first.resize(3);
    h.giveBack(back);
    std::vector<Message> again = h.older(4);
    CHECK_EQ(again.size(), (size_t)4);
    if (again.size() == 4)
    {
        CHECK_EQ(again[2].atMs, back[0].atMs);
        CHECK_EQ(again[3].atMs, back[1].atMs);
    }

    // One long run past the size rolls whole too.
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    log.began(t);
    while (std::filesystem::file_size(path) <= BroadcastLog::MAX_FILE_BYTES)
        log.added(text(id++, false, "W1AW", filler, t += 1000));
    size_t long1 = log.load(1000000).size();
    log.began(t += 1000);
    CHECK_EQ(log.load(1000000).size(), long1);

    std::filesystem::remove_all(dir, ec);
}

// A line repaired goes above the row it was put before, in the file as
// on the screen.
void testRepairedInPlace()
{
    std::filesystem::path dir = scratch("broadcast-log-place");
    std::string path = (dir / "broadcast.log").string();
    BroadcastLog log(path);
    log.began(T0);
    log.added(text(1, false, "AG7EW", "one", T0 + 1000));
    log.added(text(2, false, "KC2G", "hello", T0 + 2000));
    log.added(text(4, false, "AG7EW", "three", T0 + 3000));
    // "two" came again after its seal: above "three", below "hello".
    log.added(text(3, false, "AG7EW", "two", T0 + 60000), 4);
    log.added(text(5, false, "KC2G", "bye", T0 + 61000));
    log.ended(T0 + 62000);
    std::vector<Message> rows = log.load(100);
    CHECK_EQ(rows.size(), (size_t)6);
    if (rows.size() == 6)
    {
        CHECK_EQ(rows[0].text, std::string("one"));
        CHECK_EQ(rows[1].text, std::string("hello"));
        CHECK_EQ(rows[2].text, std::string("two"));
        CHECK_EQ(rows[3].text, std::string("three"));
        CHECK_EQ(rows[4].text, std::string("bye"));
        CHECK(rows[5].kind == Kind::Rule);
    }
    // A reader from before ignores the extra field: the old order, nothing lost.
    std::string old = readAll(path);
    CHECK(old.find("\ttwo\t4\n") != std::string::npos);
    std::filesystem::remove_all(dir);
}

void testNoPath()
{
    BroadcastLog log;
    log.began(T0);
    log.added(text(1, true, "AG7EW", "nowhere", T0));
    log.ended(T0 + 1000);
    CHECK(log.load(100).empty());
}

// A line fixed before its seal comes back fixed, marked, with what it
// first said; text with a tab in it survives.
void testEdited()
{
    std::filesystem::path dir = scratch("broadcast-log-edited");
    std::string path = (dir / "broadcast.log").string();
    BroadcastLog log(path);
    log.began(T0);
    Message mine = text(1, true, "AG7EW", "helo", T0 + 1000, MState::Sent);
    log.added(mine);
    Message theirs = text(2, false, "KC2G", "you\tthere", T0 + 2000);
    log.added(theirs);
    mine.firstText = "helo";
    mine.text = "hello";
    mine.edited = true;
    log.edited(mine);
    theirs.firstText = "you\tthere";
    theirs.text = "are you there";
    theirs.to = "AG7EW";
    theirs.ackWanted = true;
    theirs.edited = true;
    log.edited(theirs);
    log.ended(T0 + 3000);
    std::vector<Message> rows = log.load(100);
    CHECK_EQ(rows.size(), (size_t)3);
    if (rows.size() == 3)
    {
        CHECK(rows[0].text == "hello" && rows[0].edited && rows[0].firstText == "helo");
        CHECK(rows[1].text == "are you there" && rows[1].edited && rows[1].firstText == "you\tthere");
        CHECK(rows[1].to == "AG7EW" && rows[1].ackWanted);
    }
    std::filesystem::remove_all(dir);
}

} // namespace

int main()
{
    testTwoRunsAndTheirRules();
    testOddLines();
    testRolledAndPaged();
    testRepairedInPlace();
    testNoPath();
    testEdited();
    return ragtime_test::finish();
}

// The rig library against Hamlib itself: the rig list, a backend's own
// settings, and HamlibRig opening Hamlib's dummy rig (model 1, no port, no
// radio) on its thread, reading the dial, keying and unkeying, reading
// the transmit meters while transmitting and the S-meter between bursts,
// the receive filter while the Ear watches it, tuning to a frequency
// picked on the dial, and saying why when a rig
// can't be opened. Then a FlexRadio: the advice for one set up the way
// that can't work, and (with Hamlib 4.6 or later, on POSIX) its SmartSDR
// Slice model reading and tuning a pretend radio on SmartSDR's own TCP
// port, as Ragtime reads a 6000-series radio over the network.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "Check.h"
#include "ragtime/rig/HamlibRig.h"

using namespace ragtime;
using rig::HamlibRig;

namespace
{

// Waits up to five seconds for the status to pass.
bool waitFor(const HamlibRig& r, const std::function<bool(const HamlibRig::Status&)>& ok)
{
    for (int i = 0; i < 500; i++)
    {
        if (ok(r.status())) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

radio::RigSettings dummy()
{
    radio::RigSettings s;
    s.source = radio::RigSource::Hamlib;
    s.model = 1;
    s.keyRig = true;
    return s;
}

void list()
{
    CHECK(rig::available());
    CHECK(rig::hamlibVersion().rfind("Hamlib 4", 0) == 0);
    bool sawDummy = false, sawIc7100 = false, sawNet = false;
    for (const rig::Model& m : rig::models())
    {
        sawDummy = sawDummy || (m.id == 1 && m.maker == "Hamlib" && m.port == radio::PortType::None);
        sawIc7100 = sawIc7100 || (m.id == 3070 && m.maker == "Icom" && m.name == "IC-7100"
                                  && m.port == radio::PortType::Serial);
        sawNet = sawNet || m.id == 2;
    }
    CHECK(sawDummy && sawIc7100);
    // As a person reads them: IC-703 before IC-7000, Kachina before KTH.
    std::vector<rig::Model> all = rig::models();
    auto at = [&](int id) {
        for (size_t i = 0; i < all.size(); i++)
        {
            if (all[i].id == id) return (int)i;
        }
        return -1;
    };
    CHECK(at(3055) >= 0 && at(3060) >= 0 && at(3055) < at(3060));
    // NET rigctl is the Rig page's rigctld choice, read only.
    CHECK(!sawNet);
    CHECK(!rig::model(2));
    CHECK(rig::model(3070) && rig::model(3070)->name == "IC-7100");
    bool civ = false;
    for (const rig::BackendParam& p : rig::backendParams(3070)) civ = civ || p.name == "civaddr";
    CHECK(civ);
}

void dummyRig()
{
    HamlibRig r;
    CHECK(!r.status().running);
    r.start(dummy());
    CHECK(waitFor(r, [](const HamlibRig::Status& s) { return s.open && s.hz; }));
    HamlibRig::Status s = r.status();
    CHECK(s.running && !s.keyed && s.problem.empty());
    CHECK_EQ(s.rig, std::string("Hamlib Dummy"));
    // The dummy rig starts at 145 MHz.
    CHECK(s.hz && *s.hz == 145000000);
    // Its S-meter, between bursts, for the band's noise.
    CHECK(s.readsStrength);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return st.strengthReadings > 0 && st.strength; }));
    s = r.status();
    CHECK(*s.strength >= -100 && *s.strength <= 100);

    r.setPtt(true);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return st.keyed; }));
    r.setPtt(false);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return !st.keyed; }));
    CHECK_EQ(r.status().pttFailures, (uint64_t)0);

    // The dummy rig reports ALC, SWR and power in watts. Transmitting
    // (data2g-host's PTT ON, keyed by whoever) reads them, and none
    // between bursts.
    s = r.status();
    CHECK(s.readsAlc && s.readsSwr && s.readsPower);
    CHECK(!s.meters.any());
    r.setTransmitting(true);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) {
        return st.meters.alc && st.meters.swr && st.meters.watts && st.meterReadings >= 3;
    }));
    CHECK(!r.status().keyed && !r.status().meters.power);
    // A PTT change still goes first.
    r.setPtt(true);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return st.keyed; }));
    r.setTransmitting(false);
    // Still keyed by us: still transmitting.
    uint64_t before = r.status().meterReadings;
    CHECK(waitFor(r, [&](const HamlibRig::Status& st) { return st.meterReadings > before; }));
    r.setPtt(false);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return !st.keyed && !st.meters.any(); }));
    before = r.status().meterReadings;
    std::this_thread::sleep_for(std::chrono::milliseconds(3 * HamlibRig::METER_MS));
    CHECK_EQ(r.status().meterReadings, before);

    // The SWR trip: two readings over 3:1 unkey the rig and set its power
    // to 0, and it isn't keyed again, whatever it is told, until cleared.
    r.setDummyLevelForTest("RFPOWER", 0.5);
    r.setDummyLevelForTest("SWR", 3.5);
    r.setSwrTrip(true);
    r.setTransmitting(true);
    r.setPtt(true);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return st.swrTripped; }));
    s = r.status();
    CHECK(!s.keyed && s.swrTrips == 1);
    CHECK_EQ(s.tripText, std::string("SWR 3.5: Ragtime unkeyed the rig and set its power to 0 (it was 50%)"));
    std::this_thread::sleep_for(std::chrono::milliseconds(3 * HamlibRig::METER_MS));
    CHECK(!r.status().keyed && r.status().swrTrips == 1);
    // Cleared, with the antenna fixed: keyed again, as it is still told.
    r.setDummyLevelForTest("SWR", 1.2);
    r.clearSwrTrip();
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return !st.swrTripped && st.keyed; }));
    r.setPtt(false);
    r.setTransmitting(false);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return !st.keyed; }));
    r.setSwrTrip(false);

    // Keyed when stopped: unkeyed, and closed.
    r.setPtt(true);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return st.keyed; }));
    r.stop();
    CHECK(!r.status().running && !r.status().open);

    // Started again, with other settings.
    radio::RigSettings mic = dummy();
    mic.ptt = radio::PttMethod::CatMic;
    mic.pollSeconds = 2;
    r.start(mic);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return st.open; }));
    r.setPtt(true);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return st.keyed; }));
    r.setPtt(false);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return !st.keyed; }));
}

// While the Ear is open: the mode, its width and the PBT, read only.
void filterWatch()
{
    HamlibRig r;
    r.start(dummy());
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return st.open; }));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(!r.status().filter);
    r.setFilterWatch(true);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return st.filter && !st.filter->mode.empty(); }));
    HamlibRig::Status s = r.status();
    CHECK(s.filter && s.filter->widthHz > 0);
    CHECK(s.filter && s.filter->pbtIn && s.filter->pbtOut);
    r.setDummyLevelForTest("PBT_IN", 0.75);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) {
        return st.filter && st.filter->pbtIn && *st.filter->pbtIn > 0.74 && *st.filter->pbtIn < 0.76;
    }));

    // The Ear's write: the width with the mode, and the knobs, read back.
    radio::RigFilter f = *r.status().filter;
    f.widthHz = 1800;
    f.pbtIn = 0.4;
    f.pbtOut = 0.6;
    r.setFilter(f);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) {
        return st.filterWrites == 1 && st.filter && st.filter->widthHz == 1800 && st.filter->pbtOut
               && *st.filter->pbtOut > 0.59 && *st.filter->pbtOut < 0.61;
    }));
    CHECK(r.status().filterProblem.empty());
    // Not while transmitting: it waits.
    r.setTransmitting(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    f.widthHz = 2400;
    r.setFilter(f);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    CHECK_EQ(r.status().filterWrites, (uint64_t)1);
    r.setTransmitting(false);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return st.filterWrites == 2; }));

    r.setFilterWatch(false);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return !st.filter; }));

    // A write left when stopping is written before the rig closes.
    f.widthHz = 3000;
    r.setTransmitting(true);
    r.setFilter(f);
    r.stop();
}

// A frequency picked on the dial: set, read back, and held while the rig
// transmits.
void tune()
{
    HamlibRig r;
    r.start(dummy());
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return st.open && st.hz; }));
    r.setFrequency(7066000);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return st.tunes == 1 && st.hz && *st.hz == 7066000; }));
    CHECK(r.status().tuneProblem.empty());
    r.setTransmitting(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    r.setFrequency(14066000);
    r.setFrequency(14110000);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    CHECK_EQ(r.status().tunes, (uint64_t)1);
    // The newest pick goes once the burst ends.
    r.setTransmitting(false);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return st.tunes == 2 && st.hz && *st.hz == 14110000; }));
    r.stop();
}

void cantOpen()
{
    HamlibRig r;
    radio::RigSettings s = dummy();
    s.model = 999999;
    r.start(s);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return !st.problem.empty(); }));
    CHECK(r.status().problem.find("no rig model 999999") != std::string::npos);
    CHECK(r.status().problem.find("trying again") != std::string::npos);
    // Keying a rig that isn't open does nothing.
    r.setPtt(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(!r.status().keyed && !r.status().open);
    r.stop();

    s = dummy();
    s.backendConf = "nosuch=1";
    r.start(s);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return !st.problem.empty(); }));
    CHECK(r.status().problem.find("no setting \"nosuch\"") != std::string::npos);
    r.stop();

    s = dummy();
    s.model = 3070;
    s.port = "/dev/ragtime-no-such-port";
    r.start(s);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return !st.problem.empty(); }));
    CHECK(r.status().problem.find("Couldn't open the IC-7100 on /dev/ragtime-no-such-port") != std::string::npos);
    CHECK(!r.status().open);
    r.stop();
}

void flexAdvice()
{
    // The Kenwood-like 6xxx on the radio's own port: pick a slice instead.
    std::string advice = rig::flexAdvice(2036, "192.168.1.50:4992");
    CHECK(advice.find("4992 is the radio's own SmartSDR port") != std::string::npos);
    CHECK(advice.find("SmartSDR CAT port") != std::string::npos);
    bool slices = rig::model(23005).has_value();
    CHECK_EQ(advice.find("SmartSDR Slice A") != std::string::npos, slices);
    CHECK_EQ(advice.find("came in Hamlib 4.6") != std::string::npos, !slices);
    // 6xxx on a SmartSDR CAT port is right.
    CHECK(rig::flexAdvice(2036, "127.0.0.1:5002").empty());
    CHECK(rig::flexAdvice(2036, "COM5").empty());
    // A slice model names its slice.
    CHECK(rig::flexAdvice(23007, "192.168.1.50:4992").find("reads slice C") != std::string::npos);
    CHECK(rig::flexAdvice(3070, "192.168.1.50:4992").empty());

    // It ends the problem when the rig can't be opened (nothing listens
    // on this computer's 4992).
    HamlibRig r;
    radio::RigSettings s = dummy();
    s.model = 2036;
    s.keyRig = false;
    s.port = "127.0.0.1:4992";
    r.start(s);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return !st.problem.empty(); }));
    CHECK(r.status().problem.find("Couldn't open the 6xxx on 127.0.0.1:4992") != std::string::npos);
    CHECK(r.status().problem.find(". 4992 is the radio's own SmartSDR port") != std::string::npos);
    r.stop();
}

#ifndef _WIN32
// A pretend FlexRadio 6000 on SmartSDR's own TCP port, as Hamlib's
// SmartSDR backend reads one: the hello lines, then slice 0's status on
// "sub slice 0" and on "slice tune 0", and an R line answering every
// command. Hamlib always dials port 4992 and refuses 127.0.0.1 for
// SmartSDR, so it listens on 127.0.0.2:4992 (Linux answers all of
// 127/8; elsewhere, or with 4992 taken, listening() is false).
class FakeFlex
{
public:
    FakeFlex()
    {
        listener_ = socket(AF_INET, SOCK_STREAM, 0);
        int on = 1;
        setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(0x7f000002);
        a.sin_port = htons(4992);
        listening_ = listener_ >= 0 && bind(listener_, (sockaddr*)&a, sizeof a) == 0 && listen(listener_, 4) == 0;
        if (listening_) thread_ = std::thread([this]() { run(); });
    }
    ~FakeFlex()
    {
        stopping_ = true;
        if (thread_.joinable()) thread_.join();
        if (listener_ >= 0) close(listener_);
    }
    bool listening() const { return listening_; }
    std::vector<std::string> commands() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return commands_;
    }

private:
    void run()
    {
        int client = -1;
        std::string in;
        while (!stopping_)
        {
            pollfd p{client >= 0 ? client : listener_, POLLIN, 0};
            if (poll(&p, 1, 50) <= 0) continue;
            if (client < 0)
            {
                client = accept(listener_, nullptr, nullptr);
                say(client, "V1.4.0.0\nH2B9F31A7\n");
                continue;
            }
            char buf[512];
            ssize_t n = recv(client, buf, sizeof buf, 0);
            if (n <= 0)
            {
                close(client);
                client = -1;
                in.clear();
                continue;
            }
            in.append(buf, (size_t)n);
            for (size_t nl; (nl = in.find('\n')) != std::string::npos; in.erase(0, nl + 1))
            {
                std::string line = in.substr(0, nl);
                size_t bar = line.find('|');
                if (line.empty() || line[0] != 'C' || bar == std::string::npos) continue;
                std::string seq = line.substr(1, bar - 1), command = line.substr(bar + 1);
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    commands_.push_back(command);
                }
                double mhz = 0;
                if (command == "sub slice 0" || std::sscanf(command.c_str(), "slice tune 0 %lf", &mhz) == 1)
                {
                    if (mhz > 0) mhz_ = mhz;
                    char status[256];
                    std::snprintf(status, sizeof status,
                                  "S2B9F31A7|slice 0 in_use=1 RF_frequency=%.6f index_letter=A mode=DIGU "
                                  "filter_lo=0 filter_hi=3000 tx=1 active=1\n",
                                  mhz_);
                    say(client, status);
                }
                say(client, "R" + seq + "|0|\n");
            }
        }
        if (client >= 0) close(client);
    }
    static void say(int fd, const std::string& text)
    {
#ifdef MSG_NOSIGNAL
        (void)send(fd, text.data(), text.size(), MSG_NOSIGNAL);
#else
        (void)send(fd, text.data(), text.size(), 0);
#endif
    }

    int listener_ = -1;
    bool listening_ = false;
    std::atomic<bool> stopping_{false};
    double mhz_ = 14.105;
    mutable std::mutex mutex_;
    std::vector<std::string> commands_;
    std::thread thread_;
};

// Hamlib's SmartSDR Slice A on the pretend radio: the dial read, a tune
// sent and read back, and nothing keyed.
void smartSdr()
{
    if (!rig::model(23005))
    {
        std::printf("%s has no SmartSDR models: skipping the pretend Flex\n", rig::hamlibVersion().c_str());
        return;
    }
    // Refused, with the advice: SmartSDR is the radio's, never this
    // computer's.
    HamlibRig r;
    radio::RigSettings s = dummy();
    s.model = 23005;
    s.keyRig = false;
    s.port = "127.0.0.1:4992";
    r.start(s);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return !st.problem.empty(); }));
    CHECK(r.status().problem.find("not 127.0.0.1") != std::string::npos);
    r.stop();

    FakeFlex flex;
    if (!flex.listening())
    {
        std::printf("Can't listen on 127.0.0.2:4992: skipping the pretend Flex\n");
        return;
    }
    // The address alone: Hamlib adds SmartSDR's port.
    s.port = "127.0.0.2";
    r.start(s);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return st.open && st.hz; }));
    CHECK(r.status().hz && *r.status().hz == 14105000);
    CHECK_EQ(r.status().rig, std::string("FlexRadio SmartSDR Slice A"));
    r.setFrequency(7066000);
    CHECK(waitFor(r, [](const HamlibRig::Status& st) { return st.tunes == 1 && st.hz && *st.hz == 7066000; }));
    CHECK(r.status().tuneProblem.empty());
    r.stop();
    bool subscribed = false, keyed = false;
    for (const std::string& c : flex.commands())
    {
        subscribed = subscribed || c == "sub slice 0";
        keyed = keyed || c.rfind("xmit", 0) == 0;
    }
    CHECK(subscribed);
    CHECK(!keyed);
}
#endif

} // namespace

int main()
{
    list();
    dummyRig();
    filterWatch();
    tune();
    cantOpen();
    flexAdvice();
#ifndef _WIN32
    smartSdr();
#endif
    return ragtime_test::finish();
}

#include "EmbeddedModem.h"

#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>

#include <algorithm>
#include <clocale>
#include <chrono>
#include <cmath>
#include <exception>
#include <stdexcept>
#include <utility>

#include "Prefs.h"
#include "app/log.hpp"
#include "app/station.hpp"
#include "audio/card.hpp"
#include "core/arq/modes.hpp"
#include "core/arq/session.hpp"
#include "generated/config.hpp"
#include "ragtime/audio/EarAudio.h"
#include "ragtime/audio/CoreAudioVoice.h"

static_assert(data2g::config::PROFILE == "hf" && data2g::config::FS == 8000,
              "Ragtime's embedded audio and bandwidth controls require the Data2G HF profile");

namespace
{
std::optional<std::string> deviceIndex(const std::string& name, bool input)
{
    if (name.empty()) return std::nullopt;
    auto devices = input ? data2g::audio::card::input_devices() : data2g::audio::card::output_devices();
    auto device = std::find_if(devices.begin(), devices.end(), [&](const auto& d) {
        return d.channels > 0 && d.name == name;
    });
    if (device == devices.end())
        throw std::runtime_error(std::string(input ? "Input" : "Output") + " unavailable: " + name);
    return std::to_string(device - devices.begin());
}
} // namespace

EmbeddedModem::EmbeddedModem(Failure onFailure)
    : onFailure_(std::move(onFailure))
{
    data2g::app::install_arq_log();
    // Qt owns the modem's sockets and TCI timers. Service them on the same
    // thread as wxWidgets, including while a wx dialog is open.
    timer_.Bind(wxEVT_TIMER, [this](wxTimerEvent&) {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        if (!station_ || !station_->failed()) return;
        stop();
        onFailure_("The Data2G engine stopped. Reopen Settings and apply the modem configuration to restart it.");
    });
}

EmbeddedModem::~EmbeddedModem()
{
    stop();
}

std::string EmbeddedModem::start(const StationPrefs& prefs)
{
    stop();
    try
    {
        data2g::app::Args args;
        args.listen = prefs.hostAddress();
        args.mycall = prefs.callsign;
        args.kiss_port = prefs.kissPort;
        args.command_port = prefs.commandPort;
        args.rig = false; // Ragtime's existing rig control owns Hamlib.
        args.tci = prefs.modem.tci;
        if (args.tci) args.tci_host = prefs.modem.tciHost;
        if (args.kiss_port < 1 || args.kiss_port > 65535 || args.command_port < 1 || args.command_port > 65534
            || args.kiss_port == args.command_port || args.kiss_port == args.command_port + 1)
            return "Choose three distinct KISS, command and session data ports under Settings > Station.";
        if (auto error = data2g::app::check(args)) return *error;
        if (!args.tci)
        {
            args.input_device = deviceIndex(prefs.modem.input, true);
            args.output_device = deviceIndex(prefs.modem.output, false);
            ragtime::audio::reserveCoreAudioRadioDevices(prefs.modem.input, prefs.modem.output);
        }
        station_ = std::make_unique<data2g::app::Station>(std::move(args));
        station_->start();
        if (!timer_.Start(10)) throw std::runtime_error("Could not start the modem event timer.");
        return {};
    }
    catch (const std::exception& e)
    {
        stop();
        return e.what();
    }
}

void EmbeddedModem::stop()
{
    timer_.Stop();
    station_.reset();
    earSamples_ = 0;
    ragtime::audio::releaseCoreAudioRadioDevices();
}

void EmbeddedModem::readEarAudio(ragtime::audio::EarAudio& ear)
{
    constexpr int rate = data2g::config::FS;
    if (!station_)
    {
        ear.hostAudio("receive audio", nullptr, 0, rate);
        return;
    }
    const auto& args = station_->args();
    const std::string name = args.tci ? "TCI radio audio" : "radio audio";
    std::uint64_t total = 0;
    bool transmitting = false;
    // Data2G taps 100 ms engine blocks and reports the direction of the
    // newest one. Limit the read to that block so one batch has one tag.
    const auto tail = station_->input_tail(rate / 10, &total, &transmitting);
    const auto count = std::min<std::uint64_t>(tail.size(), total - earSamples_);
    earSamples_ = total;
    const std::vector<float> audio(tail.end() - count, tail.end());
    ear.hostAudio(name, audio.data(), audio.size(), rate, transmitting);
}

std::optional<ModemConstellation> EmbeddedModem::takeConstellation()
{
    if (!station_) return std::nullopt;
    auto plot = station_->take_constellation();
    if (!plot) return std::nullopt;
    return ModemConstellation{std::move(plot->mode), plot->tones, plot->live, plot->lost,
                              std::move(plot->symbols), std::move(plot->points)};
}

std::optional<ModemTelemetry> EmbeddedModem::telemetry() const
{
    if (!station_ || !station_->running() || station_->failed()) return std::nullopt;
    const auto& args = station_->args();
    const auto link = station_->link();
    const auto audio = station_->counters();
    ModemTelemetry out;
    out.audio = args.tci ? "TCI · " + args.tci_host : "Core Audio";
    using data2g::arq::SessionState;
    switch (link.state)
    {
        case SessionState::CONNECTING: out.link = "Connecting to " + link.peer; break;
        case SessionState::CONNECTED:
            out.link = "Connected to " + link.peer + " · BW" +
                       data2g::arq::bw_name(std::clamp(link.cap, 0, 2));
            break;
        case SessionState::DISCONNECTING: out.link = "Disconnecting from " + link.peer; break;
        case SessionState::LISTEN: out.link = "Listening"; break;
        default: out.link = data2g::arq::state_name(link.state); break;
    }
    out.mode = link.mode;
    if (const auto* mode = data2g::arq::mode(link.mode))
        out.mode += " · " + std::to_string(std::lround(data2g::arq::width_hz(*mode))) + " Hz";
    out.busy = station_->busy();
    out.ptt = station_->ptt();
    out.rxBytes = link.rx_bytes;
    out.txBytes = link.tx_bytes;
    out.overflows = audio.overflows;
    out.underruns = audio.underruns;
    out.late = audio.late;
    out.dropped = audio.dropped;
    out.decodeDropped = audio.decode_dropped;
    out.backlogSeconds = audio.backlog_s;
    return out;
}

std::vector<ModemBurst> EmbeddedModem::takeBursts()
{
    if (!station_) return {};
    auto bursts = station_->take_bursts();
    std::vector<ModemBurst> out;
    out.reserve(bursts.size());
    for (auto& burst : bursts)
    {
        const auto when = std::chrono::duration_cast<std::chrono::milliseconds>(burst.when.time_since_epoch()).count();
        out.push_back({std::move(burst.mode), burst.lost, burst.snr_db, when,
                       burst.n_cw, burst.decoded, burst.failed});
    }
    return out;
}

std::vector<std::string> EmbeddedModem::devices(bool input)
{
    std::vector<std::string> names;
    for (const auto& device : input ? data2g::audio::card::input_devices() : data2g::audio::card::output_devices())
        if (device.channels > 0) names.push_back(device.name);
    return names;
}

std::string EmbeddedModem::version()
{
    return DATA2G_VERSION;
}

int EmbeddedModem::runWithRuntime(int argc, char** argv, int (*run)(int, char**))
{
    // Qt needs an application object even though wx owns the main loop.
    int qtArgc = 1;
    char* qtArgv[] = {argv[0], nullptr};
    QCoreApplication runtime(qtArgc, qtArgv);
    std::setlocale(LC_NUMERIC, "C");
    return run(argc, argv);
}

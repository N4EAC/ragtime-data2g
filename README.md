# Ragtime

A plain, friendly chat and file-transfer client for
[Data2G](https://github.com/arodland/Data2G), the HF data modem by Andrew
Rodland, KC2G. Ragtime is a sibling of
[Glissando](https://github.com/baitisj/glissando), Jeff Baitis' (AG7EW)
chirp-mode chat app, and is named for the same musical family and for the
ragchew.

Ragtime helps you keep in touch. Every burst it hears teaches it when,
and on which band, each of your friends gets through, so it can tell you
who is likely around now, and the best time of day to check in to the
D2G net to find them.

Status: 0.4.0 development build. Ragtime connects to data2g-host,
chats on the D2G broadcast group. Current features include:
- "Globus," which plots your friend's maidenhead coordinates
   on a globe,
- "Gearshift," a graphical representation of different Data2G modes,
  which aids in the selection of the optimal rate for chatting with
  the group. It reads thresholds from Data2G v0.5.1 and newer. An older
  host uses the local `mode_thresholds.json` file described below.
- Peer-to-peer sessions, which feature automatic rate negotiation and
  file transfer.
- Group file transfer which includes repair rounds.
- Progressive inline images using JPEG 2000, or the much more impressive
  [Cool-Chic](https://github.com/Orange-OpenSource/Cool-Chic) low complexity
  neural image codec.
- Reactions to messages.
- "Recent work," under the station list: the stations you hear and
  work, the ones likely around at this hour first, from two weeks of
  the path log (who was heard when, and on which band).
- Voice messages, coded with [sotto](https://github.com/baitisj/sotto),
  Jeff's vocoder: 300 bytes for each second of speech at 2,400 bit/s,
  150 at 1,200, 75 at 600 or 50 at 400, every rate with sotto's
  4-8 kHz hiss band, sent as a file the way pictures go, and in a ragchew played
  while it is still arriving. The deck's Rate picks. Voice over Data2G
  is data, so it goes wherever Data2G runs; at 2,400 it is a gear
  5-and-up feature, and the slower rates take it lower.

## Data2G

Andrew Rodland (KC2G) designed Data2G as a free, open-source HF data modem for
amateur radio experimentation. It delivers up to 6,400 bit/s net throughput on a
good channel, with a 2,400 Hz channel for full speed and an optional 500 Hz
bandwidth cap. It supports connected links with hybrid ARQ and broadcast groups
with forward error correction. Its VARA HF-compatible command interface and KISS
ports let applications use the modem for messaging, file transfers, and packet
radio. [Data2G project README](https://github.com/arodland/Data2G#readme)

## Screenshots

**Ragtime on macOS.** The frequency selector menu, including per-frequency
notes, alongside the Broadcast tab and built-in Data2G modem.

![Ragtime on macOS with the ordered frequency selector menu](docs/images/readme/ragtime-macos.png)

Use **View > Search** (Cmd+F) to search Broadcast and earlier sessions.
The Search tab opens on request; the usual Broadcast, Session and Files tabs
keep their order. Chat history is retained and loaded a page at a time, and
older pictures decode in the background. Clear hides Broadcast messages
across restarts while preserving the log: Search can explicitly review them;
closing the search strip restores the cleared view.

## Downloads

The current published macOS release is 0.4.0 beta 5; published ZIP and DMG packages
are listed on the release page below. The macOS app bundles Data2G v0.5.3
and sotto v3.8 for Apple Silicon with macOS 27 or later. Packages are ad hoc
signed and have no Apple notarization.
[Ragtime macOS releases](https://github.com/N4EAC/ragtime-data2g/releases) · [Changelog](CHANGELOG.md)

The macOS app opens with a 2.8-second rotating Earth animation. Click or
press Esc to skip; Reduce Motion uses a still frame.

Transparent color and monochrome logos for video overlays are available in
[branding assets](assets/branding) and the release's branding ZIP.

Linux and Windows source builds connect to an external `data2g-host`.
Enter its address and ports under **Settings > Station**.

The gearshift takes its thresholds from Data2G's `MODES` response when
available. With an older host, it reads `data2g/codes_data/mode_thresholds.json`
from your Data2G checkout under `$DATA2G_HOME`, or a `Data2G` or `data2g`
folder in your home folder or its `src`, `git`, `code`, `projects`,
`Projects`, `repos` or `dev` folder. Otherwise load the file with **Load
thresholds...** in the gearshift. See [docs/GEARBOX.md](docs/GEARBOX.md).

## Building

On Ubuntu 24.04:

    sudo apt-get install build-essential cmake pkg-config libwxgtk3.2-dev libgtk-3-dev libhamlib-dev libpipewire-0.3-dev \
        libopenjp2-7-dev libwebp-dev libheif-dev libheif-plugin-libde265 git
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(nproc)
    ctest --test-dir build --output-on-failure
    ./build/app/ragtime

On Fedora 44:

    sudo dnf install wxGTK-devel libde265-devel libheif-devel openjpeg-devel libwebp-devel pipewire-devel hamlib-devel

### macOS

The integrated modem uses Data2G v0.5.3 plus upstream receiver and session fixes at commit
`6dafe77ad0a8b3f693fd3d7d6d5dc9883d25f092`. Use a separate checkout for
this build. The bundled modem uses the HF profile; the experimental FM
profile is not enabled. [Data2G v0.5.3](https://github.com/arodland/Data2G/releases/tag/v0.5.3)

    git clone https://github.com/arodland/Data2G.git ../Data2G-ragtime
    git -C ../Data2G-ragtime checkout 6dafe77ad0a8b3f693fd3d7d6d5dc9883d25f092

Install the build dependencies with Homebrew:

    brew install cmake ninja pkg-config wxwidgets hamlib openjpeg webp libheif
    brew install --skip-link qtbase qtsvg qtdeclarative qtwebsockets
    cmake -S . -B build-macos -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=27.0 \
        -DRAGTIME_DATA2G_SOURCE_DIR="$PWD/../Data2G-ragtime" \
        -DCMAKE_PREFIX_PATH="$(brew --prefix qtbase)" \
        -DQT_ADDITIONAL_PACKAGES_PREFIX_PATH="$(brew --prefix qtwebsockets)"
    cmake --build build-macos --parallel
    ctest --test-dir build-macos --output-on-failure
    open build-macos/app/Ragtime.app

Ragtime keeps its wxWidgets window and links to Data2G's native station
library. Set your callsign and locator in **Settings > Station**, then
enable **Use the built-in Data2G modem** in **Settings > Modem**. Choose
the radio's input and output devices, or select **TCI** and enter the
radio's address. With a sound card and Hamlib, configure radio keying
under **Settings > Rig**. On first use of a sound card, allow microphone
access when macOS asks.

The modem listens only on loopback, using the KISS and command ports
under **Settings > Station**, plus the session data port at command + 1.
Use free ports if another modem is already running. Ragtime remembers the
external address for when you switch back. Pass
`-DRAGTIME_EMBED_DATA2G=OFF` to build without the modem library.

To stage an app with its shared libraries bundled:

    mkdir -p dist
    ditto build-macos/app/Ragtime.app dist/Ragtime.app
    packaging/macos/deploy.sh dist/Ragtime.app

The built-in modem uses Core Audio or TCI for radio audio. In
**Settings > Voice**, choose your microphone and speaker and use the
**Test** buttons to check them. Voice messages use the bundled sotto codec.
Ragtime leaves the built-in modem's sound devices out of the voice lists.
On macOS 14.2 and later it also checks for devices used by local Data2G
processes. The Ear follows the built-in modem over Core Audio or TCI and
shows its outgoing waveform in grey during transmit. You can also select
a Core Audio input in the Ear's
**Listen to** menu. On macOS 14.2 and later the Ear can discover a local
external Data2G sound-card input.

In source builds with the modem monitor, **View > Data2G modem monitor**
opens a separate window with the built-in
modem's receive constellation, channel state, session traffic, last-burst SNR,
and audio and decoder counters. Below them, a receive/decode history shows
UTC time, mode/codewords, SNR and the result reported by Data2G: OK, Lost,
Undecoded, or Incomplete data. Undecoded can also mean the burst was not
addressed to this station. The list keeps the newest 500 bursts; **Clear**
clears the display while reception continues. **Help > Open session logs folder** opens the
per-callsign chat logs.

The built-in Data2G modem supports roll calls immediately after connecting;
you do not need to receive another station first. External modems retain
their capability check and fall back to a normal check-in until Hand support
is confirmed.

**Settings > Rig** shows all Hamlib models and can filter by manufacturer.
For serial connections, pick your radio's `/dev/cu.*` port. The packaged app
includes both Hamlib tools. To list supported rigs or start a local daemon:

    dist/Ragtime.app/Contents/MacOS/rigctl -l
    dist/Ragtime.app/Contents/MacOS/rigctld -T 127.0.0.1 -m MODEL -r /dev/cu.DEVICE

The second command uses your Hamlib model number and serial port. Select
**Read the dial from rigctld** in Ragtime to use that daemon.

The macOS connection bar includes an editable frequency field with presets.
Enter **7100** (kHz) and press **Enter** to tune to **7.100 MHz** through the
rig connection configured in **Settings > Rig**. Explicit units also work:
**7100000 Hz** or **7.100 MHz**. Selecting a preset tunes immediately.
The band button also opens the upstream hot-frequency menu using the same
file. Ctrl with the mouse wheel steps 1 kHz; Shift steps 10 kHz.
CAT readings update the field while idle; an entry you are editing stays
intact until Enter sends it or Escape restores the radio reading.
The initial selection is **7.066 MHz**. New lists use upstream's defaults:
**1.806, 3.566, 5.3465, 5.360, 5.4035, 7.066, 7.110, 10.146,
14.066, 14.110, 18.106, and 28.066 MHz**. The 5.360 MHz entry includes
upstream's `9.15 watts ERP Maximum` note. Untouched five-frequency lists
upgrade automatically; custom lists are preserved. Ragtime remembers the selection
after the radio confirms the change and restores it when reopened.
Reopening restores the selection without automatically tuning the radio.

On macOS, both **File > Edit frequency list** and the frequency menu's
**Edit** action open a graphical text editor (normally TextEdit).
Use **File > Edit frequency list** to edit
`~/Library/Application Support/Ragtime/frequencies.txt`. Enter one
frequency per line in MHz, with a decimal point. Lines may contain
`# comments`; comments after a frequency appear beside it in both menus.
Saved changes load automatically. An invalid file leaves the
last valid list available and reports the offending line in the dropdown's
tooltip. The last-used frequency stays available if removed from the file.
With `--config PATH`, the list is stored at `PATH.frequencies.txt`.

Frequency changes are unavailable while disconnected or transmitting.
The built-in modem's transmit audio and Auto ALC are capped at 0 dB,
matching Data2G v0.5.3.
**About Ragtime** lists the Ragtime version, built-in Data2G version, loaded
sotto format, and bundled sotto revision.

Recent Work has a **Clear** button. Clearing persists across restarts and
keeps the archived path and session logs; stations appear again as new
activity arrives.

For Android, [android/README.md](android/README.md) has the development
APK, its setup and the fake-host tests. The phone app has a native UI of
its own over the same C++ core: chat, files, pictures and the gearshift,
with voice still to come.

### Libraries

These are all the libraries the build looks for. The package names are
Ubuntu 24.04's (Debian uses the same `-dev` names); other distributions
have them under their own names.

| Library | Ubuntu package | For | Without it |
| --- | --- | --- | --- |
| A C++17 compiler (GCC 13, 14 and 16 tested), CMake 3.16 or newer, pkg-config | `build-essential cmake pkg-config` | The build | Required |
| wxWidgets 3.2 (GTK 3) | `libwxgtk3.2-dev` | The window | `-DRAGTIME_APP=OFF` builds only the core, the rig library and their tests |
| GTK 3 | `libgtk-3-dev` | The ham phrase highlight in the entry lines | Required for the app |
| Hamlib | `libhamlib-dev` | Settings > Rig, opening the rig with Hamlib | `-DRAGTIME_HAMLIB=OFF` |
| PipeWire 0.3 headers | `libpipewire-0.3-dev` | The Ear's listening | `-DRAGTIME_PIPEWIRE=OFF` |
| OpenJPEG 2.5 and libwebp | `libopenjp2-7-dev libwebp-dev` | Inline pictures: JPEG 2000 both ways, WebP read | `-DRAGTIME_PICTURES=OFF` |
| libheif, with its libde265 plugin | `libheif-dev libheif-plugin-libde265` | Pictures read from HEIF (phones' `.heic`) | `-DRAGTIME_HEIF=OFF` |
| git | `git` | Building libsotto for voice when configuring (below) | `-DRAGTIME_BUILD_SOTTO=OFF`, or a warning and no voice |

Warnings are errors by default. If a compiler we haven't tried stops
the build on a warning, `-DRAGTIME_WERROR=OFF` lets it finish, and we'd
like to hear about it.

Ragtime reads JPEG, PNG and TIFF through wxWidgets, never with those
libraries itself. They come in with wxWidgets, which is built against them: the
linker wants whichever `libjpeg`, `libpng`, `libtiff` and `libwebp`
your wxWidgets was built with, and any version does as long as it is
that one. On Ubuntu 24.04, `libwxgtk3.2-dev` pulls them in, libjpeg as
libjpeg-turbo 2.1.5 (`libjpeg-turbo8`, giving `libjpeg.so.8`), so there
is nothing more to install.

At run time the app needs the same libraries without `-dev`
(`libwxgtk3.2-1t64`, `libhamlib4t64`, `libopenjp2-7`, `libwebp7`,
`libheif1`), and HEIF needs `libheif-plugin-libde265` (the AppImage
carries it). The Ear loads
`libpipewire-0.3.so.0` (`libpipewire-0.3-0t64`) only when it opens, and
needs PipeWire to be the sound server. On Windows it listens through
WASAPI, which needs nothing installed.

### Voice messages: libsotto

Voice messages are coded with [sotto](https://github.com/baitisj/sotto).
Ragtime loads it at run time as a shared library and never links it, so
it builds without it: with no libsotto the voice cards still draw, and
Play and the deck say libsotto is missing. Ragtime needs sotto v3.8,
at the commit pinned in `packaging/sotto.sh`: it codes 16 kHz speech
with a 4-8 kHz hiss band (codec 2), and plays clips from the earlier
v3.6 (codec 1) too. An older libsotto (v3.6 or v3.7) is passed over.
From the pinned commit it also codes sotto 1200, 600 and 400 with the
hiss band (codecs 3, 4 and 5), from and to 16 kHz.

**The build makes it for you.** On Linux, configuring Ragtime (`cmake -S
. -B build`) builds libsotto when none is found, in about ten seconds,
into `build/build-sotto/out/`, where the program finds it. It runs once,
and again only when the pinned commit changes. It needs `git`, `cmake`,
`make` and a C compiler. If it can't build libsotto
(offline, say), configuring goes on with a warning, and the next
configure tries again. `-DRAGTIME_BUILD_SOTTO=OFF` turns it off.

To build it by hand instead:

    packaging/build-sotto.sh      # build-sotto/out/libsotto.so.0 and libcodec2.so.1.2

Either way, it clones sotto, builds the Codec 2 fork that sotto carries
(Ubuntu's own `libcodec2` won't do), and leaves `libsotto.so.0` in a
`build-sotto/out/` folder. Ragtime looks for libsotto in these places,
in order:

1. `$RAGTIME_SOTTO`, the full path to the file, for a libsotto kept
   anywhere else.
2. Beside the `ragtime` program.
3. `../lib/` from the program (where the AppImage keeps it).
4. `build-sotto/out/` in any of the four folders above the program:
   the build folder's (from configuring) or the checkout's (from
   `build-sotto.sh`).
5. The system's library path (`libsotto.so.0`; `sotto.dll` on Windows).

It takes the first that loads and says `sotto v3.8`; one from an older
pin is passed over.

libsotto finds its `libcodec2` in its `build-sotto` folder, so leave
that folder where it is. The microphone deck shows which sotto loaded (hover
over its codec line to see the path), or why none did.

The AppImage carries libsotto and its Codec 2 (`make-appimage.sh` runs
`build-sotto.sh`). The Windows zip carries `sotto.dll` and
`libcodec2.dll` beside `ragtime.exe`: `make-zip.sh` cross-builds them
with `packaging/windows/build-sotto.sh` at the same pinned commit, or
takes them from `RAGTIME_SOTTO_DLL_DIR`. (Zips up to 0.3.1-beta had none, so
they couldn't record or play voice: issue #166.)

The helper scripts need a few more tools: `tools/ear/pretend_radio.sh`
wants `pipewire-pulse`, `pulseaudio-utils` (`pactl`),
`pipewire-bin` (`pw-play`, `pw-cat`) and `python3-numpy`;
`tools/gcc16/check.py` wants Docker; and `tools/icon/make_icons.sh`
wants `librsvg2-bin` and `imagemagick`.

### Packages

    packaging/linux/make-appimage.sh      # dist/Ragtime-<version>-...-x86_64.AppImage
    packaging/windows/make-zip.sh         # dist/Ragtime-<version>-...-windows-x86_64.zip

The Windows build cross-compiles on Ubuntu 24.04 with
`g++-mingw-w64-x86-64-posix` and builds its own static wxWidgets and
Hamlib (which needs `autoconf automake libtool`); with
Wine installed, `ctest --test-dir build-win` runs the unit tests as
Windows programs.

## Frequencies

Where to try Ragtime under Part 97: places in the US HF data segments
clear of the busy weak-signal spots. Nobody owns these, and nothing here
is a schedule. Listen first, and move if someone is there.

In USB a Data2G burst sits about 0.3 to 2.7 kHz above the dial (less for
narrow modes). So keep the dial at least 0.3 kHz above the low edge of a
range and 2.7 kHz below its high edge. Figures are the signal, in MHz.

| Band | Try | Avoid nearby |
| --- | --- | --- |
| 80 m | 3.581–3.585 (room for one wide burst) | WSPR 3.5700–3.5702; FT8 3.573–3.576; FT4 3.575–3.578; JS8Call 3.578–3.581; automatic stations 3.585–3.600 |
| 40 m | 7.081–7.099, 7.105–7.122 | WSPR 7.0400–7.0402; FT4 7.0475–7.0505; FT8 7.074–7.077; JS8Call 7.078–7.081; automatic stations 7.100–7.105 |
| 30 m | 10.143–10.149 | JS8Call 10.130–10.133; FT8 10.136–10.139; FT4 10.140–10.143; WSPR 10.1400–10.1402 |
| 20 m | 14.083–14.095, 14.112–14.147 | FT8 14.074–14.077; JS8Call 14.078–14.081; FT4 14.080–14.083; WSPR 14.0970–14.0972; NCDXF beacons 14.100; automatic stations 14.095–14.112 |
| 17 m | 18.085–18.097 | FT8 18.100–18.103; FT4 and JS8Call 18.104–18.107; WSPR 18.1060–18.1062; automatic stations 18.105–18.110; NCDXF beacons 18.110 |
| 15 m | 21.081–21.089, 21.100–21.139, 21.152–21.199 | FT8 21.074–21.077; JS8Call 21.078–21.081; automatic stations 21.090–21.100; WSPR 21.0960–21.0962; FT4 21.140–21.143; NCDXF beacons 21.150 |
| 12 m | 24.895–24.911 | FT8 24.915–24.918; FT4 24.919–24.922; JS8Call 24.922–24.925; automatic stations 24.925–24.930, with WSPR 24.9260–24.9262; NCDXF beacons 24.930 |
| 10 m | 28.081–28.119 | FT8 28.074–28.077; JS8Call 28.078–28.081; automatic stations 28.120–28.189, with WSPR 28.1260–28.1262 and FT4 28.180–28.183; beacons 28.190–28.300 |

60 m is channels and one small segment (47 CFR 97.303(h)). Data goes
on a channel only with the carrier 1.5 kHz below its centre: the dial at
exactly 5.3305, 5.3465, 5.3715 or 5.4035 MHz in USB, at up to 100 W
ERP. The band segment 5.3515–5.3665 MHz takes data anywhere, at up to
9.15 W ERP (about 9 W into a dipole); keep the dial at 5.3545–5.3635 MHz.
FT8 is at 5.357 dial. With "Receive only outside the US data segments"
ticked, Ragtime holds sending anywhere else on 60 m.

How these were picked (2026-10-26):

- **What's busy.** PSK Reporter's mode counts for the two hours before
  21:40 UTC: FT8 3.2 million reports, FT4 148,000, WSPR 128,000, CW
  14,000, JS8Call 8,900, VarAC 4,500, RTTY 95, PSK31 51. So the table
  steers around FT8, FT4, WSPR and JS8Call, and treats the old PSK31 and
  Olivia spots as free. VarAC's calling spots (3.595, 7.105, 10.133,
  14.105, 18.106, 21.105, 24.927, 28.105 MHz dial) are left open on
  purpose: Data2G is VarAC's spiritual successor, and its users are the
  people most likely to try Ragtime. RTTY contesters don't report to PSK
  Reporter, and they fill the RTTY areas on contest weekends.
- **Where.** The "avoid" figures are the usual WSJT-X and JS8Call
  dial frequencies plus the audio those modes use. The ARRL band
  plan's RTTY and data areas come first; the 17 m and 12 m ranges sit
  just below them. The ranges also clear Glissando's presets (3.570,
  7.067, 14.067, 18.097, 21.067, 24.911, 28.067 MHz dial).
- **Other countries' band plans.** Part 97 allows 2.8 kHz data
  throughout these segments, but IARU band plans ask for less in places,
  and DX stations follow them. Region 1 (Europe, Africa) keeps all of
  30 m to 500 Hz. Region 2 (the Americas) allows 2.7 kHz at 10.140–10.150,
  and asks for 500 Hz at 3.580–3.600, 14.070–14.099, 21.070–21.090,
  21.120–21.149 and 28.070–28.190 MHz. Use a narrow mode there when
  working DX. In other Region 2 countries 7.060–7.100 and 14.112 MHz up
  are SSB, so expect phone there.
- **Licences.** Data on these bands needs a General licence or higher,
  except on 10 m, where Technicians may send data up to 28.300 MHz.
- Ragtime turns receive only when the dial is outside a US data
  segment, or less than 3 kHz inside its edge, and says so in the
  connection bar.
- Unattended stations (automatic control) must stay in the narrower
  segments of 97.221(b).

Sources: [ARRL band plan](https://www.arrl.org/band-plan),
[IARU Region 2 band plan](https://www.iaru.org/wp-content/uploads/2020/10/IARU-Region-2-Band-plan.pdf),
[PSK Reporter statistics](https://pskreporter.info/cgi-bin/pskstats.pl).

## The idea

- **Welcome to the D2G net.** D2G is a net that never closes. You check
  in, net control asks for traffic, and the traffic gets passed. Every
  burst teaches the net who hears whom.
- **Sometimes we ragchew.** When operators want to talk, they enter a
  point to point ragchew on purpose and leave it on purpose.

Ragtime speaks the Data2G host protocol over TCP, to the built-in modem
on macOS or to a separately running `data2g-host`. The Ear listens through
PipeWire on Linux, WASAPI on Windows or Core Audio on macOS. It can also
display the built-in modem's receive and transmit audio directly.
Voice messages use your microphone and speaker (Settings > Voice).
It reads the radio's dial from rigctld, or opens the rig itself with
Hamlib (Settings > Rig), and can key it when data2g-host does, for rigs
whose drivers can't be opened twice (off by default). Through Hamlib or
rigctld, the Ear can also set the rig's receive filter: drag the edges
on the waterfall, or the notch out of its bay, and the change goes back
after 15 s unless you keep it (off by default).
Messages are signed, never encrypted, as Part 97 requires.

## Plan

| Document | What |
| --- | --- |
| [docs/HOW_SEALS_WORK.md](docs/HOW_SEALS_WORK.md) | Seals in plain words, with pictures: what is inside one, how repairs work, and what a late joiner asks for |
| [docs/GEARBOX.md](docs/GEARBOX.md) | The gearshift: how a Data2G mode is picked from the Submode Map |
| [CLAUDE.md](CLAUDE.md) | Working notes and ground rules for contributors |

## Licence

MIT. See [LICENSE](LICENSE).

## Credits

Original Ragtime by Jeff Baitis (AG7EW). Upstream macOS port by Tim Vetter.
This fork adds the built-in Data2G modem and its macOS integration.

The packaged sotto library is pinned to commit
`e707134fd7f9e2721b55b9d983b77a8aa5593552`. It retains the v3.8 frame
formats with upstream v3.10 encoder pitch improvements. Ragtime continues
using the desktop encoders and decoders; the new text-to-speech prototype
is not bundled.

Voice messages are coded with [sotto](https://github.com/baitisj/sotto),
by Jeff Baitis (AG7EW). Ragtime loads it at run time as a shared library
(`libsotto.so`, `sotto.dll`), because sotto's analysis and synthesis come
from David Rowe's [Codec 2](https://github.com/drowe67/codec2), which is
LGPL-2.1; the rest of sotto is MIT. sotto's codebook was trained on
Codec 2's own test recordings and on these datasets, both under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/):

- [Speech Commands](https://arxiv.org/abs/1804.03209), Pete Warden,
  Google, 2018.
- [LibriSpeech](https://www.openslr.org/12) (dev-clean), Vassil Panayotov,
  Guoguo Chen, Daniel Povey and Sanjeev Khudanpur, 2015, from LibriVox
  recordings.

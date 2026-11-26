# Changelog

Changes to the Ragtime fork with bundled Data2G for macOS. Dates use
America/New_York. Entries summarize this fork's packaged releases; the
upstream projects retain their own histories.

## Unreleased — 2026-11-26

- Keep About under Help on macOS and show the build date, with plain
  component revisions and developer credits.

- Integrate upstream Ragtime through `b4ba6e5`: optional history Search,
  read-only past sessions, retained/paged chat history and background picture decoding.
- Add upstream FlexRadio network support and Ear controls for notch frequency,
  IF SHIFT, and FT-891 filter widths; retain manual CAT entry and presets.
- Include upstream seal/Hand-window scheduling and gearshift text fixes.
- Update built-in Data2G HF to `6dafe77`: carrier-resistant BUSY detection,
  CPM receiver improvements, robust session starts/disconnects and KISS compatibility fixes.
- Retain sotto `e707134`, the upstream-recommended speech codec. Later sotto
  commits add watermark tools and do not change the bundled speech codec.
- Preserve persistent Clear across paged/rolled history and temporarily show
  cleared history only for an explicit Search review. Queued messages remain visible.
- Keep the bundled modem monitor, RX/TX Ear, audio devices, startup/icon,
  plain About revisions, logs and macOS controls.

## 0.4.0 beta 5 — 2026-11-24

- Update built-in Data2G to `95c94b1`, including carrier-aware CPM noise
  floors/SNR estimates and receive-preview/lock-reporting corrections.
- Explicitly require the HF modem profile and 8 kHz audio contract. Upstream's
  new FM profile remains separate; the HF protocol stays at version 15.
- Verify Ragtime `00ac994` and sotto `e707134` remain current upstream.
- Remove absolute Homebrew search paths from packaged transitive libraries.
- Preserve the modem monitor, RX/TX Ear tap, CAT controls, voice, logs,
  persistent Clear behavior and the bundled app's independent identity.

## 0.4.0 beta 4 — 2026-11-23

- Show each component's revision code alongside its version in About.
- Validate prebuilt macOS codec revisions and include the matching codec
  and provenance in the app bundle.
- Integrate Ragtime upstream through `00ac994`, retaining the macOS UI,
  startup animation, manual CAT entry, clear controls, logs, and bundled modem.
- Update built-in Data2G to `9894b63`: receiver acquisition improvements,
  noise/SNR estimation fixes, and session turnaround/control-reply fixes.
- Update sotto to `e707134`, with v3.10 encoder pitch improvements while
  retaining v3.8 voice frame formats. The TTS prototype is not bundled.

## 0.4.0 beta 3 — 2026-11-21

- Add a brief macOS startup animation with a rotating Earth and streaming
  binary digits. It closes after 2.8 seconds, supports click/Esc to skip,
  and uses a still frame with macOS Reduce Motion. The badge-free title reads
  "Ragtime" with "for macOS +Data2G" underneath.
- Keep the frequency entry at a fixed width while sizing the preset popup
  to show full comments. File reloads wait until the popup is closed.
- Add transparent color and monochrome Earth logos for video overlays,
  available in the source assets folder and a separate branding ZIP.

## 0.4.0 beta 2 — 2026-11-21

- Use upstream's 12 frequency defaults and show per-frequency comments in
  the macOS preset dropdown. Untouched legacy defaults upgrade automatically;
  custom lists, manual entry, and CAT confirmation are preserved.
- Replaced the macOS icon with the approved glass Earth and binary fade,
  with Data2G and smaller Ragtime text inside one badge.
- Updated the README screenshot to show the ordered frequency menu.

## 0.4.0 beta — 2026-11-21

- Show About's developers as plain names and callsigns, with no dropdowns
  or contribution descriptions.
- Open frequency-list editing in the default graphical macOS text editor
  instead of launching nano in Terminal.
- Integrated upstream Ragtime 0.4.0, retaining the bundled Data2G modem
  and editable Enter-to-tune frequency field.
- Added the upstream hot-frequency menu and wheel tuning. On macOS it
  shares the existing frequency file and confirmation path; CAT VFO/PTT
  checks remain in place.
- Replaced About's licence expander with a plain `License: MIT` line.
  Ragtime's upstream MIT licence is unchanged.
- Replaced the maple-leaf icon with an Apple silhouette using the same
  teal pixel treatment, dark background, and Data2G badge.

## 0.3.1 beta 6 — 2026-11-21

- Integrated upstream Ragtime through `6cde44e`, including macOS desktop
  fixes, background-message notifications, and explicit session disconnect
  when the app quits.
- Updated bundled Data2G to `759ea35` (v0.5.3 plus configuration-discovery
  fixes). Added a regression check that standalone configuration files
  cannot override the embedded modem's explicit arguments.
- Kept sotto v3.8 at `598f1bc` and preserved the built-in modem, audio-device
  reservations, RX/TX Ear tap, frequency entry, modem monitor, logs,
  persistent Clear controls, roll-call fix, custom icon, and rigctl tools.
- Integrated upstream macOS audio and codec tests, while retaining the
  bundled modem's audio bridge. The external-modem build uses upstream's
  native Core Audio backend.
- Improved codec lookup after relocation and normalized copied libraries'
  install names to keep the macOS bundle self-contained.
- Validation: 53 integrated regression tests, 11 targeted external-profile
  checks, and an application GUI smoke test passed. Verified the packaged
  signature, codec round trip, icon, rig tools, and library dependencies.

## 0.3.1 beta 5 — 2026-11-20

- Enabled roll calls immediately after the built-in modem connects,
  without first requiring reception of another station.
- Preserved capability detection and ordinary check-in fallback for
  external modems.
- Removed the developer-specific Hand-support warning and tooltip text;
  clear stale fallback notices after a successful roll-call check-in.

## 0.3.1 beta 4 — 2026-11-20

- Updated sotto v3.8 to `598f1bc`, retaining desktop encoder/decoder and
  frame compatibility. The upstream revision adds a slim encoder and table
  hooks for embedded hardware.
- Made the macOS frequency control editable: entering `7100` in kHz and
  pressing Enter tunes to 7.100 MHz. Explicit Hz, kHz, and MHz units work.
- Kept CAT readback active without overwriting an unfinished entry;
  Escape restores the radio reading. Preset selection remains available.

## 0.3.1 beta 3 — 2026-11-19

- Added receive/decode history to the modem monitor, showing UTC time,
  mode/codewords, SNR, and Data2G's OK, Lost, Undecoded, or Incomplete result.
- Retained every burst returned in a poll, with the newest 500 rows and a
  Clear button. Automatically fit the columns when the window resizes.

## 0.3.1 beta 2 — 2026-11-19

- Updated upstream Ragtime, bundled Data2G to v0.5.3 (`ede34e7`), and sotto
  v3.8 to `6686fe4`.
- Added editable macOS frequency presets, confirmed-frequency persistence,
  and active-VFO handling for Hamlib and rigctld-compatible servers.
- Added persistent Recent Work clearing without deleting archived logs.
- Matched the built-in modem's transmit-audio and Auto ALC limit to
  Data2G's 0 dB maximum.
- Listed the Ragtime, Data2G, and sotto versions in About.

## 0.3.1 beta — 2026-11-15

- Integrated upstream Ragtime 0.3.1 and upstream picture-preview and
  broadcast-repair fixes, preserving the bundled macOS modem integration.

## 0.2.0 beta 2 — 2026-11-15

- Updated Data2G's Hand timing and handling of expired roll-call windows.
- Moved the Data2G constellation and modem telemetry into a separate
  monitor window to keep the main window uncluttered.

## 0.2.0 beta — 2026-11-14

- Initial Apple Silicon macOS app with built-in Data2G and sotto voice.
- Added Core Audio/TCI integration, macOS voice devices and Ear support,
  Hamlib/rigctld control, and packaged rigctl tools.
- Added the macOS app icon, chat/session logging, broadcast clearing,
  and ZIP/DMG downloads.

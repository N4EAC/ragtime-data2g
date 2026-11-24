# Upstream verification — 2026-11-24

| Component | Upstream head checked | Result |
|---|---|---|
| Ragtime | `00ac994e63fd6e9b303c6ccafefa5e6b1fca8306` | Already integrated; no newer commits |
| Data2G | `95c94b1c1b85edbdff6d1e6828d3087e67ccfeef` | Adopted from previous `9894b63` |
| sotto | `e707134fd7f9e2721b55b9d983b77a8aa5593552` | Already integrated; no newer commits |

Data2G's changes include per-tone CPM noise floors and carrier-aware SNR
measurement, fixed-timing receive preview for truncated frames, burst lock
reporting, host option precedence, simulation poll scheduling, and a separate
FM service profile. This build explicitly selects HF, keeps protocol 15 and
8 kHz audio, and does not expose FM-only controls or waveforms.

The bundled app links the updated native station. The independent Ragtime
macOS edition connects to an external Data2G host and carries no modem engine.
Both retain their existing macOS features and independent app identifiers.

Sources: https://github.com/baitisj/ragtime,
https://github.com/arodland/Data2G, https://github.com/baitisj/sotto.

## Validation

- All 31 updated Data2G native HF tests passed.
- Previous (`9894b63`) and current (`95c94b1`) HF engines decoded each other's
  audio exactly in both directions for six representative OFDM/FSK modes
  (`ack-1f`, `qpsk-r1/2`, `n4-ack-2f`, `n10-qpsk-r1/3`,
  `w48-16qam-r1/2`, `w48-64l-r1/2`), two codewords per burst.
- The standalone app initialized a real updated external HF host, opened
  KISS/command/session sockets, and quit while leaving the host running.
  Reproduce with `python3 tests/macos/ExternalData2gTest.py --app APP --host HOST`.
- The bundled telemetry/decode monitor smoke check passed using the updated
  modem and a silent loopback TCI bench. RX/TX Ear taps passed their
  Ragtime regression tests.
- Ragtime regression/UI suites and frequency/clear persistence checks passed
  independently for both editions. These checks use simulated radio/audio;
  physical radio TX and ALC are not part of this verification.

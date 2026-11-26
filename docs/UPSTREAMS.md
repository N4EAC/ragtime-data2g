# Bundled macOS upstream integration — 2026-11-26

| Component | Revision | Result |
|---|---|---|
| Ragtime | `b4ba6e56f290bcf9d5fb2bc6dc7124dc6b843269` | Integrated through upstream main |
| Data2G | `6dafe77ad0a8b3f693fd3d7d6d5dc9883d25f092` | Built-in HF engine updated from `95c94b1` |
| sotto speech codec | `e707134fd7f9e2721b55b9d983b77a8aa5593552` | Retained at Ragtime upstream's recommended pin |
| sotto repository main | `73129464f98f7da6e2a1e6d7412494a01f2ae46f` | Later additions are independent watermark tools; no speech-codec changes |

Ragtime adds optional history Search, read-only earlier sessions, retained and
paged chat history, background historical picture decoding, FlexRadio network
support, and more Ear rig controls. Search follows Broadcast, Session and Files
only while open. The macOS Clear action remains persistent, including rolled
logs; an explicit search can review hidden rows without undoing Clear.

Data2G includes carrier-resistant presence BUSY detection, CPM receiver fixes
and search caching, robust session-start/disconnect handling, and additional
VARA KISS frame types. HF profile, protocol 15 and 8 kHz audio remain unchanged.
The upstream engine source is unmodified in an independent pinned checkout.

The independent Ragtime macOS and standalone Data2G apps are outside this
update. The installed bundled app is backed up before replacement. This integration is packaged as macOS 0.4.0 beta 6.

Validation uses native tests and simulated audio/radios. Physical-radio TX,
ALC and particular radio models require station testing.

Sources: https://github.com/baitisj/ragtime,
https://github.com/arodland/Data2G, https://github.com/baitisj/sotto.

Validation: 63 Ragtime regression/widget tests and 31 Data2G native HF tests
passed. Cross-version audio payloads passed in both directions for six modes.
Deep Search/Clear, manual CAT entry, pictures, the live modem monitor and native
Help/About were checked. The final packaged modem returned HAND END EXPIRED
and shut down cleanly. Physical-radio TX/ALC requires station testing.

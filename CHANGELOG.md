# Changelog

## v0.3.1 — 2026-11-09

Community test build `n_261109_ar04`, based on Mayhem nightly `n_261109`.

- Stabilized START AUTO / STOP AUTO captions and limited redundant display repainting.
- Restricted capture loss checks to the retained prebuffer and current recording.
- Joined the capture worker before reading final counters and buffer state.
- Ended captures on silence below Trigger without the rearm margin holding them open.
- Preserved damaged captures, incremented SKIP, and automatically rearmed after cooldown. Clean captures continue to delayed transmit; genuine file errors still stop.
- Packaged matching firmware, 104 PPMA apps, two standalone apps and 70 baseband images.
- Added a screen-only interface walkthrough and screenshots for community testers.

Owner reports the repeater working well so far. Meshtastic opens; message exchange remains unverified. This is not an official Mayhem or Meshtastic release.

## v0.3.0 — 2026-11-09

- Rebased on the November 9 Mayhem nightly and rebuilt matching SD applications.
- Fixed Meshtastic startup/RAM handling and restored the PFUT internal flashing component.
- Cached the custom version to keep application compatibility stable across regeneration.

## v0.2.1 / v0.2.0 — development builds

- Expanded repeater controls, modes, automatic operation and file support.
- Added the experimental Meshtastic app under Transceivers.
- These builds omitted the internal flash writer; updating from them requires the packaged USB procedure.

## v0.1.0 — initial development build

- Introduced the H4M Auto Repeater application.

See the README and packaged documentation for mode, file and radio limitations.

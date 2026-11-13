# Changelog

## ar05 - 2026-11-13

Test firmware `n_261109_ar05` for HackRF One + PortaPack H4M.

- Synced Mayhem upstream through 2b7e3b4023b46d4e2c557ffa205a1e02a8caef12.
- Fixed custom channels with empty keys using synthetic encryption instead of plaintext.
- Added custom channel support for raw 128-bit/256-bit hexadecimal keys and padded Base64 PSKs.
- Added an Import key button for a key-only SD-card .TXT file and expanded typed key entry to 64 characters.
- Preserved the existing custom passphrase format and primary-channel default key behavior.
- Added rate-limited receive decode failure diagnostics.
- Corrected the updater package to USTAR with explicit directories, matching APPS/BASEBAND files, and the firmware image last.
- Included the matching firmware .bin, Windows USB tools/drivers, backup and read-back verification launcher, instructions and SHA-256 checksums.

Validation: ARM build, layout, runtime memory, installation and update-archive checks passed.
Craig successfully flashed the corrected archive. In a bench test with LILYGO T3 Meshtastic 2.7.26, the PortaPack imported a 256-bit Base64 key and received the LILYGO test text. The LILYGO received "256" from the PortaPack. A subsequent reply was not observed, so repeated-delivery reliability remains under investigation. This test covered the secondary test channel on Short Fast at 433.125 MHz. Other presets, direct-message PKC, arbitrary keys and manual 256-bit typing require further hardware testing.


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

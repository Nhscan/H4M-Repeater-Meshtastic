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

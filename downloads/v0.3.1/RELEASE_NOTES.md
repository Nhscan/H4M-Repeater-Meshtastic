# H4M Repeater + Meshtastic v0.3.1 — community test build

Firmware: **n_261109_ar04**. Base: Mayhem nightly **n_261109**, HackRF One + PortaPack H4M.

The owner reports the repeater working well so far. Meshtastic opens, but message exchange and wider interoperability still need testing. This is a preliminary community test build.

Changes:

- Stable Start/Stop captions and reduced redundant LCD repainting.
- Capture-quality checks limited to the retained prebuffer/current event.
- End-silence handling below Trigger, without noise holding each burst to its maximum duration.
- Captures with genuine gaps are kept/skipped, and Auto rearms; clean captures proceed to TX.
- Matching firmware, 104 PPMA apps, two standalone apps and 70 SD baseband images.
- Preserves the v0.3.0 Meshtastic startup/RAM and internal-updater fixes.

From working v0.3.0: copy `SD_CARD` contents, then flash **H4M_AR04.ppfw.tar** through Flash Utility. Reboot and confirm **n_261109_ar04**.

From this project's v0.2.x: use the included Windows USB installer or the documented HackRF-mode command because those builds omitted their internal flash writer.

Read the repository README and the package's START-HERE.txt for limitations and test-report details. No comprehensive physical-hardware validation is claimed.

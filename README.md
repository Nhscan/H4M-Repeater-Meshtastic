# H4M Auto Repeater + Meshtastic

Latest test build: [ar05 downloads and changes](https://github.com/Nhscan/H4M-Repeater-Meshtastic/releases/tag/ar05). Includes the complete SD-card and Windows flash package.

Community test firmware for **HackRF One + PortaPack H4M**, based on Mayhem nightly **n_261109**.

![Auto Repeater demo](docs/h4m/media/demo-preview.gif)

▶️ **[Watch the full screen walkthrough video (5 min)](docs/h4m/media/H4M-v0.3.1-screen-demo.mp4)**

## What's in this build

This firmware adds two new apps to the HackRF/PortaPack H4M, both found under **Transceivers (TRX)**:

### 📡 Auto Repeater
A store-and-forward "parrot" repeater for the PortaPack. It sits on a receive frequency and listens. When a signal rises above your trigger level, it records it to the SD card. When the signal stops, it waits a short delay and transmits the recording back out, on the same frequency or on a separate TX frequency. Then it rearms for the next one. It works with raw IQ (any signal), with voice modes (NFM, FM, AM, USB, LSB, WFM), and with OOK remotes. In OOK mode it can recognize many common key-fob and sensor protocols live and export Flipper Zero `.sub` files. An **Auto level** button sets the trigger from the background noise for you. **REC / TX / SKIP** counters show what it's doing at a glance.

### 💬 Meshtastic (experimental, needs testers!)
A native Meshtastic client that runs entirely on the HackRF, using software-defined LoRa with no external LoRa radio. It has **Chat**, **Nodes**, **Data** and **Setup** tabs. You can join a mesh by matching region, modem preset, channel and key with your other nodes, then send and receive text messages, NodeInfo and telemetry.

> ⚠️ **Meshtastic needs more testing.** I don't own a Meshtastic device, so I haven't been able to test the HackRF against a real Meshtastic node. On my H4M the app opens and sends NodeInfo/telemetry, but **sending and receiving messages with real nodes is untested**. If you own a Meshtastic radio, please try it and [open an issue](https://github.com/Nhscan/H4M-Repeater-Meshtastic/issues) with your results (region, preset, frequency, and whether each side saw the other's messages).

## Screenshots

| Auto Repeater | Repeater settings | Transceivers menu |
|:---:|:---:|:---:|
| ![Auto Repeater main screen](docs/h4m/media/01-repeater-ook.png) | ![Auto Repeater settings](docs/h4m/media/02-repeater-settings.png) | ![Transceivers menu with both apps](docs/h4m/media/03-transceivers.png) |

| Meshtastic chat | Meshtastic radio setup |
|:---:|:---:|
| ![Meshtastic chat tab](docs/h4m/media/04-meshtastic-chat.png) | ![Meshtastic radio setup](docs/h4m/media/05-meshtastic-radio.png) |

## Download v0.3.1

**[⬇️ Download the complete v0.3.1 ZIP](https://github.com/Nhscan/H4M-Repeater-Meshtastic/raw/next/downloads/v0.3.1/H4M-Repeater-Meshtastic-v0.3.1.zip)** · [Release page](https://github.com/Nhscan/H4M-Repeater-Meshtastic/releases/tag/v0.3.1) · [Screen walkthrough video](docs/h4m/media/H4M-v0.3.1-screen-demo.mp4)

Custom firmware version: **`n_261109_ar04`**. The ZIP includes firmware, matching SD applications and baseband files, Windows USB recovery tools, source patch, checksums and instructions.

This is a **community test build**, not an official Mayhem or Meshtastic release. The owner reports that the repeater works well in preliminary H4M testing. Meshtastic opens on the H4M, but it has not been tested against a real Meshtastic node yet because the owner doesn't have a Meshtastic device. Testers with Meshtastic radios are needed. Other hardware, radio modes and long-running behavior have not been comprehensively validated.

## Install

1. Extract the ZIP and copy everything **inside `SD_CARD`** to the root of the microSD card. Replace matching files and retain your other data.
2. If you already run this project's **v0.3.0 / `n_261109_ar03`**, open **Utilities → Flash Utility** and install **`H4M_AR04.ppfw.tar`**.
3. Let flashing finish, restart, and confirm that About shows **`n_261109_ar04`**.
4. Open **Auto Repeater** or **Meshtastic** under **Transceivers / TRX**.

**Updating from this project's v0.2.0 or v0.2.1 requires USB:** those builds omitted the internal flashing component. Put the H4M in **HackRF mode**, connect USB, and run **`USB_FLASH_WINDOWS/START_H4M.bat`** from the extracted package. Wait for its successful read-back verification, then restart. The ZIP also contains DFU recovery and Linux/macOS instructions.

Keep this custom firmware together with its matching **104 `.ppma` applications** and `BASEBAND` folder. Official `n_261109` SD applications use a different version identifier. A version mismatch can display an **outdated `.ppma`** message.

See [complete installation instructions](docs/h4m/INSTALL.txt). No SD-card formatting is required.

## Auto Repeater

- Half-duplex store-and-forward operation with separate RX and TX frequencies, or TX following the received frequency.
- Signal-level trigger, end-silence timer, maximum capture duration, **1–30 second TX delay**, cooldown and automatic rearming.
- Save/discard after successful transmission, record-only mode, repeat count/gap, RX/TX gain and amplifier controls.
- Raw IQ; NFM, FM, AM, USB, LSB and mono WFM voice modes; optional voice monitoring.
- OOK mode adds live frame/protocol recognition where an included decoder recognizes the signal.
- Optional receive-frequency hopping; recording and transmission hold the selected channel.
- Flipper `.sub` **RAW/BinRAW** playback for supported OOK/2FSK presets. Compatible OOK recordings can also export as Flipper RAW `.sub` files.

### v0.3.1 repeater fixes

The Start/Stop caption updates only when it changes. Other text refreshes at most five times per second, reducing redundant LCD writes.

Capture-quality checks cover the retained prebuffer and current event rather than the entire idle listening session. Silence below the selected trigger ends a recording; the optional rearm margin no longer holds a short event open below the noise floor.

A capture with real sample gaps is saved and counted as **SKIP**, then Auto returns to listening after cooldown. It is not automatically transmitted or exported as `.sub`. Clean captures follow the normal delay, transmission and rearm sequence. Actual file/SD errors still stop with their specific error.

For a first test, choose one known RX frequency, disable hopping, keep Save checked, and use **Settings → Auto level** with the remote quiet. Watch the **REC / TX / SKIP** counts.

## Meshtastic

An experimental native software-LoRa application is included. Match **region, preset, channel and key** with another Meshtastic node.

The LoRa startup/RAM fix is retained from v0.3.0. The v0.3.1 receiver/transmitter DSP binaries are byte-identical to v0.3.0. Opening the app does not by itself establish successful RF communication.

**Limitations:** SF12 reception is disabled. The SF7 ring holds 92 symbols and can truncate long packets. The SF8–11 streaming window is 3.5 symbols; timing and RF interoperability need physical testing. [Meshtastic guide](docs/h4m/MESHTASTIC.md).

## Signal and file limitations

Raw IQ repeats a recorded waveform; protocol recognition is not required for raw recording. OOK recognition identifies supported frame formats, not every manufacturer/model. It does not decrypt or synthesize rolling codes, and replay acceptance is not guaranteed.

`.sub` export is amplitude-sliced OOK RAW, not a universal IQ-to-Flipper conversion. Voice/FSK/arbitrary RF recordings cannot all be exported as OOK. Encoded protocol/key `.sub` files and custom presets are not synthesized by the importer; use supported RAW/BinRAW captures.

## Testing and feedback

Please include these details when sharing test results:

- Hardware and firmware version shown in About.
- Mode, RX/TX frequency, sample rate, trigger and end-silence settings.
- Whether hopping, Save, Record only or an amplifier was enabled.
- REC / TX / SKIP counts and the exact on-screen error, if any.
- Whether a second receiver/node observed the expected transmission.

Build checks cover the 1 MiB firmware checksum and version, all 104 PPMA identifiers/checksums, external-app layout, runtime LoRa memory budget, updater residency and final archive ordering. Host regressions cover repeated automatic cycles, capture-loss windows, UI repaint suppression, file conversion, a synthetic Princeton IQ waveform and imported Meshtastic protocol tests. These checks do not replace physical RF/SD/UI validation.

## Source and building

This repository contains the complete modified firmware source. The base is Mayhem tag [`nightly-tag-2026-11-09`](https://github.com/portapack-mayhem/mayhem-firmware/releases/tag/nightly-tag-2026-11-09), commit `706fd7f91e0318fc4643e8d31f5087ececb312c4`.

Use GNU Arm Embedded **GCC 9.2.1**, Ninja and the dependencies listed in [BUILD.txt](docs/h4m/BUILD.txt):

```sh
git clone --recurse-submodules --branch next https://github.com/Nhscan/H4M-Repeater-Meshtastic.git
cd H4M-Repeater-Meshtastic
python3 tools/update_external_tier_ld.py
cmake -S . -B build -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DMAYHEM_CUSTOM_VERSION=n_261109_ar04
cmake --build build --target oci -j6
python3 tests/auto_repeater/check_layout.py build
python3 tests/auto_repeater/check_runtime_memory.py build
python3 tests/auto_repeater/check_install.py build n_261109_ar04
```

The custom version is cached so CMake regeneration does not silently change app compatibility. The internal flash writer **PFUT** is included in SPI alongside the LoRa and factory images.

## Credits and license

- [PortaPack Mayhem](https://github.com/portapack-mayhem/mayhem-firmware) and its contributors. The original project README is preserved as [README-UPSTREAM.md](README-UPSTREAM.md).
- Experimental Meshtastic software-LoRa implementation by **Alexey Verhogladov**, imported from [`mamapapa1/mayhem-firmware`, `mesh-test-1`](https://github.com/mamapapa1/mayhem-firmware/tree/mesh-test-1), commit `9d028dd349b9f38dcc897ca2a071d91adec3e851`. Existing copyright notices are retained.
- OOK recognizers and their upstream Flipper-related acknowledgements remain in the source headers. LoRa framing sources acknowledge gr-lora_sdr and related research.
- Windows recovery utilities/drivers are redistributed unchanged from the official `n_261109` firmware asset, with provenance in the package credits.

See [LICENSE](LICENSE), [LICENSE.GPL-2.0-or-later](LICENSE.GPL-2.0-or-later), and the individual source-file notices.

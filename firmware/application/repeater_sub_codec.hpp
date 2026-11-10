// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <cstring>
namespace auto_repeater {
struct SubInfo {
    int64_t frequency{0};
    uint32_t deviation{0}, te{0};
    bool binary{false};
};
// The source supplies bool get(char&). The sink supplies pulse(level, us).
// Fixed buffers; RAW_Data lines may be arbitrarily long without growing RAM.
template <typename Source, typename Sink>
const char* parse_sub(Source& source, Sink& sink, SubInfo& info) {
    char key[40]{}, value[128]{};
    bool filetype = false, protocol = false, preset = false, any = false;
    uint32_t bits = 0;
    char ch = 0;
    while (source.get(ch)) {
        if (ch == '\r' || ch == '\n') continue;
        size_t k = 0;
        do {
            if (ch == ':') break;
            if (ch == '\n') break;
            if (k + 1 < sizeof(key)) key[k++] = ch;
        } while (source.get(ch));
        key[k] = 0;
        if (ch != ':') continue;
        const bool raw = !std::strcmp(key, "RAW_Data"), bin = !std::strcmp(key, "Data_RAW");
        if (raw || bin) {
            if (!filetype || !protocol || !preset || info.frequency < 1000000 || info.frequency > 6000000000LL) return "Unsupported SUB header";
            if (raw == info.binary) return "SUB protocol/data mismatch";
            if (bin && (!info.te || !bits)) return "Invalid BinRAW TE/bit count";
            uint32_t number = 0;
            bool negative = false, token = false;
            auto emit = [&]() -> bool {
                if (!token) return true;
                if (raw) {
                    if (!number || number > 10000000) return false;
                    if (!sink.pulse(!negative, number)) return false;
                } else {
                    if (number > 255) return false;
                    for (int b = 7; b >= 0 && bits; b--, --bits)
                        if (!sink.pulse((number >> b) & 1, info.te)) return false;
                }
                any = true;
                number = 0;
                negative = false;
                token = false;
                return true;
            };
            while (source.get(ch)) {
                if (ch == '\n') {
                    if (!emit()) return "Invalid SUB pulse data";
                    break;
                }
                if (ch == ' ' || ch == '\t' || ch == '\r') {
                    if (!emit()) return "Invalid SUB pulse data";
                    continue;
                }
                if (raw && ch == '-' && !token) {
                    negative = true;
                    token = true;
                    continue;
                }
                const int digit = ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10
                                                                  : ch >= 'a' && ch <= 'f'   ? ch - 'a' + 10
                                                                                             : -1;
                const uint32_t radix = raw ? 10 : 16;
                if (digit < 0 || uint32_t(digit) >= radix || number > 10000000) return "Invalid SUB number";
                number = number * radix + digit;
                token = true;
            }
            if (!emit()) return "Invalid SUB pulse data";
            continue;
        }
        size_t v = 0;
        while (source.get(ch) && ch != '\n') {
            if (ch != '\r' && v + 1 < sizeof(value)) value[v++] = ch;
        }
        value[v] = 0;
        char* text = value;
        while (*text == ' ' || *text == '\t') ++text;
        if (!std::strcmp(key, "Filetype"))
            filetype = !std::strcmp(text, "Flipper SubGhz RAW File") || !std::strcmp(text, "Flipper SubGhz Key File");
        else if (!std::strcmp(key, "Protocol")) {
            protocol = !std::strcmp(text, "RAW") || !std::strcmp(text, "BinRAW");
            info.binary = !std::strcmp(text, "BinRAW");
        } else if (!std::strcmp(key, "Preset")) {
            preset = false;
            if (!std::strcmp(text, "FuriHalSubGhzPresetOok270Async") || !std::strcmp(text, "FuriHalSubGhzPresetOok650Async")) {
                info.deviation = 0;
                preset = true;
            }
            if (!std::strcmp(text, "FuriHalSubGhzPreset2FSKDev2380Async")) {
                info.deviation = 2380;
                preset = true;
            }
            if (!std::strcmp(text, "FuriHalSubGhzPreset2FSKDev476Async")) {
                info.deviation = 47600;
                preset = true;
            }
        } else if (!std::strcmp(key, "Frequency") || !std::strcmp(key, "TE") || !std::strcmp(key, "Bit_RAW")) {
            uint64_t n = 0;
            for (auto* p = text; *p; p++) {
                if (*p < '0' || *p > '9' || n > 6000000000ULL) return "Invalid SUB header number";
                n = n * 10 + (*p - '0');
            }
            if (!std::strcmp(key, "Frequency"))
                info.frequency = n;
            else if (!std::strcmp(key, "TE")) {
                if (n > 10000000) return "Invalid SUB TE";
                info.te = n;
            } else {
                if (n > 16000000) return "BinRAW bit count too large";
                if (bits) return "BinRAW segment truncated";
                bits = n;
            }
        }
    }
    return !any ? "SUB has no supported pulse data" : bits ? "BinRAW data truncated"
                                                           : nullptr;
}
}  // namespace auto_repeater

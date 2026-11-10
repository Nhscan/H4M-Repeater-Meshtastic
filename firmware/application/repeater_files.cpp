// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef REPEATER_FILE_HOST_TEST
#include "../../tests/auto_repeater/host_file_shim.hpp"
#include "repeater_sub_codec.hpp"
#else
#include "repeater_jobs.hpp"
#include "repeater_sub_codec.hpp"
#include "metadata_file.hpp"
#include "dsp_types.hpp"
#include <cstdio>
#include <cstring>
#endif
namespace auto_repeater {
namespace {
struct Source {
    File f{};
    char buffer[256]{};
    size_t pos{0}, count{0};
    bool failed{false};
    bool get(char& c) {
        if (pos == count) {
            auto r = f.read(buffer, sizeof(buffer));
            if (r.is_error()) {
                failed = true;
                return false;
            }
            if (!r.value()) return false;
            pos = 0;
            count = r.value();
        }
        c = buffer[pos++];
        return true;
    }
};
constexpr int16_t sine[256] = {0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793, 12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594, 23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956, 30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757, 32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285, 32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571, 30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683, 27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731, 23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868, 18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279, 12539, 11793, 11039, 10278, 9512, 8739, 7962, 7179, 6393, 5602, 4808, 4011, 3212, 2410, 1608, 804, 0, -804, -1608, -2410, -3212, -4011, -4808, -5602, -6393, -7179, -7962, -8739, -9512, -10278, -11039, -11793, -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530, -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594, -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790, -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956, -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971, -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757, -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285, -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571, -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683, -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731, -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868, -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279, -12539, -11793, -11039, -10278, -9512, -8739, -7962, -7179, -6393, -5602, -4808, -4011, -3212, -2410, -1608, -804};
struct PulseWriter {
    File& file;
    FileJob& job;
    SubInfo& info;
    complex16_t buffer[256]{};
    size_t used{0};
    uint64_t us{0}, samples{0};
    uint32_t phase{0};
    bool flush() {
        if (!used) return true;
        auto r = file.write(buffer, used * sizeof(complex16_t));
        const bool ok = r.is_ok() && r.value() == used * sizeof(complex16_t);
        used = 0;
        return ok;
    }
    bool pulse(bool high, uint32_t duration) {
        if (job.cancel) return false;
        us += duration;
        if (us > 30000000) return false;
        const uint64_t target = (us + 1) / 2;  // 500 ksample/s, cumulative rounding avoids drift.
        const uint32_t step = (uint64_t(info.deviation) << 32) / 500000;
        while (samples < target) {
            if (info.deviation) {
                phase += high ? step : -step;
                buffer[used++] = {int16_t(sine[((phase >> 24) + 64) & 255] / 2), int16_t(sine[phase >> 24] / 2)};
            } else
                buffer[used++] = {int16_t(high ? 16384 : 0), 0};
            ++samples;
            if (used == 256) {
                if (job.cancel || !flush()) return false;
            }
        }
        return true;
    }
};
bool copy_range(File& source, File& dest, uint64_t offset, uint64_t length, FileJob& job) {
    if (source.seek(offset).is_error()) return false;
    uint8_t buffer[1024];
    while (length && !job.cancel) {
        const auto n = std::min<uint64_t>(length, sizeof(buffer));
        auto r = source.read(buffer, n);
        if (r.is_error() || r.value() != n) return false;
        auto w = dest.write(buffer, n);
        if (w.is_error() || w.value() != n) return false;
        length -= n;
    }
    return !job.cancel;
}
bool export_ook(FileJob& job, const std::filesystem::path& path) {
    File source;
    if (source.open(path)) return false;
    auto metadata = read_metadata_file(get_metadata_path(path));
    if (!metadata) return false;
    complex16_t samples[256];
    uint32_t peak = 0;
    uint64_t done = 0;
    const auto size = source.size();
    while (done < size && !job.cancel) {
        auto r = source.read(samples, std::min<uint64_t>(size - done, sizeof(samples)));
        if (r.is_error() || !r.value()) return false;
        done += r.value();
        for (size_t i = 0; i < r.value() / 4; i++) {
            const int32_t re = samples[i].real(), im = samples[i].imag();
            const uint32_t p = int64_t(re) * re + int64_t(im) * im;
            if (p > peak) peak = p;
        }
    }
    if (!peak || job.cancel) return false;
    auto output = path;
    output.replace_extension(u".SUB");
    File dest;
    if (dest.create(output)) return false;
    char line[160]{};
    std::snprintf(line, sizeof(line), "Frequency: %llu", static_cast<unsigned long long>(metadata->center_frequency));
    if (dest.write_line("Filetype: Flipper SubGhz RAW File") || dest.write_line("Version: 1") || dest.write_line(line) || dest.write_line("Preset: FuriHalSubGhzPresetOok650Async") || dest.write_line("Protocol: RAW")) return false;
    uint32_t on = uint64_t(peak) * job.threshold_percent / 100, off = on * 8 / 10;
    bool high = false;
    uint64_t run = 0, total = 0;
    uint32_t transitions = 0, valid_high_runs = 0;
    size_t pos = 0;
    auto emit = [&](uint64_t count, bool level) -> bool {
        if (!count) return true;
        const uint64_t duration = (count * 1000000 + metadata->sample_rate / 2) / metadata->sample_rate;
        if (!duration) return true;
        if (level && duration >= 40) ++valid_high_runs;
        if (!pos) {
            std::strcpy(line, "RAW_Data:");
            pos = 9;
        }
        pos += std::snprintf(line + pos, sizeof(line) - pos, " %s%llu", level ? "" : "-", static_cast<unsigned long long>(duration));
        if (pos > 110) {
            auto e = dest.write_line(line);
            pos = 0;
            if (e) return false;
        }
        return true;
    };
    if (source.seek(0).is_error()) return false;
    while (total < size && !job.cancel) {
        auto r = source.read(samples, std::min<uint64_t>(size - total, sizeof(samples)));
        if (r.is_error() || !r.value()) return false;
        total += r.value();
        for (size_t i = 0; i < r.value() / 4; i++) {
            const int32_t re = samples[i].real(), im = samples[i].imag();
            const uint32_t p = int64_t(re) * re + int64_t(im) * im;
            const bool next = high ? p >= off : p >= on;
            if (next != high) {
                if (!emit(run, high)) return false;
                run = 0;
                high = next;
                ++transitions;
            }
            ++run;
        }
        job.percent = 50 + total * 50 / size;
    }
    if (!emit(run, high) || (pos && dest.write_line(line))) return false;
    dest.close();
    if (transitions < 2 || valid_high_runs < 4 || transitions > 200000 || job.cancel) {
        delete_file(output);
        return false;
    }
    return true;
}
}  // namespace
void process_repeater_files(FileJob& job) {
    if (job.kind == FileJob::ImportSUB) {
        Source source;
        File dest;
        SubInfo info;
        if (source.f.open(job.input) || dest.create(job.output)) {
            job.error = "Cannot open SUB conversion";
            return;
        }
        PulseWriter writer{dest, job, info};
        const auto* error = parse_sub(source, writer, info);
        if (error || source.failed || !writer.flush() || job.cancel) {
            job.error = error ? error : "SUB conversion canceled";
            dest.close();
            delete_file(job.output);
            return;
        }
        dest.close();
        job.frequency = info.frequency;
        if (write_metadata_file(get_metadata_path(job.output), {info.frequency, 500000})) {
            job.error = "Cannot save converted IQ";
            return;
        }
        job.ok = true;
        job.error = nullptr;
        job.percent = 100;
        return;
    }
    if (job.kind == FileJob::FinishRing) {
        File source, dest;
        if (source.open(job.input) || dest.create(job.output) || source.size() < job.prefix || job.end > job.prefix) {
            job.error = "Cannot finalize prebuffer";
            return;
        }
        if (!copy_range(source, dest, job.end, job.prefix - job.end, job) || !copy_range(source, dest, 0, job.end, job) || !copy_range(source, dest, job.prefix, source.size() - job.prefix, job)) {
            job.error = "Prebuffer copy failed";
            return;
        }
        dest.close();
        source.close();
        delete_file(job.input);
        job.percent = 50;
    }
    job.ok = true;
    job.error = nullptr;
    if (job.export_sub && !export_ook(job, job.output)) {
        auto partial = job.output;
        partial.replace_extension(u".SUB");
        delete_file(partial);
        job.error = "IQ saved; SUB needs clean OOK";
    }
    if (job.cancel) {
        job.ok = false;
        job.error = "File processing canceled";
    }
    job.percent = 100;
}
}  // namespace auto_repeater

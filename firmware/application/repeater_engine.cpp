// SPDX-License-Identifier: GPL-2.0-or-later
#include "repeater_engine.hpp"
#include "baseband_api.hpp"
#include "portapack.hpp"
#include "io_convert.hpp"
#include "io_wave.hpp"
#include "metadata_file.hpp"
#include "oversample.hpp"
#include "replay_schedule.hpp"
#include "ui_spectrum.hpp"
#include "string_format.hpp"
#include "audio.hpp"
#include "file_path.hpp"
#include "../baseband/fprotos/subghztypes.hpp"
#include <cstring>
#include <cstdio>
using namespace portapack;
namespace auto_repeater {
namespace {
struct Lock {
    Mutex& m;
    explicit Lock(Mutex& v)
        : m(v) { chMtxLock(&m); }
    ~Lock() { chMtxUnlock(); }
};
// Bound playback to PCM data, excluding WAV LIST/INFO tags. Pad one sample
// when needed so ReplayThread's 4-byte buffer alignment cannot cut the tail.
class PCMReader : public WAVFileReader {
   public:
    uint32_t remaining{0};
    bool padded{false};
    bool load(const std::filesystem::path& p) {
        if (!open(p)) return false;
        remaining = data_size();
        return true;
    }
    File::Result<File::Size> read(void* buffer, File::Size count) override {
        const auto take = std::min<File::Size>(count, remaining);
        File::Size used = 0;
        if (take) {
            auto r = FileReader::read(buffer, take);
            if (r.is_error()) return r.error();
            used = r.value();
            remaining -= used;
        }
        if (!remaining && !padded && used % 4 == 2 && count - used >= 2) {
            std::memset(static_cast<uint8_t*>(buffer) + used, 0, 2);
            used += 2;
            padded = true;
        }
        return used;
    }
};
}  // namespace
class RingWriter : public stream::Writer {
   public:
    explicit RingWriter(uint32_t capacity)
        : capacity{capacity} { chMtxInit(&mutex); }
    Optional<File::Error> create(const std::filesystem::path& path) { return file.create(path); }
    File::Result<File::Size> write(const void* data, File::Size bytes) override {
        Lock lock{mutex};
        if (recording) return file.write(data, bytes);
        const auto* p = static_cast<const uint8_t*>(data);
        File::Size left = bytes;
        while (left) {
            const auto n = std::min<File::Size>(left, capacity - position);
            auto r = file.write(p, n);
            if (r.is_error()) return r.error();
            if (r.value() != n) return File::Error{FR_DISK_FULL};
            p += n;
            left -= n;
            position += n;
            valid = std::min<uint64_t>(capacity, valid + n);
            if (position == capacity) {
                position = 0;
                auto q = file.seek(0);
                if (q.is_error()) return q.error();
            }
        }
        return bytes;
    }
    bool trigger() {
        Lock lock{mutex};
        prefix = valid;
        end = position;
        recording = true;
        return !file.seek(prefix).is_error();
    }
    bool reset() {
        Lock lock{mutex};
        position = 0;
        valid = 0;
        return !file.seek(0).is_error() && !file.truncate().is_error();
    }
    uint64_t prefix{0}, end{0};

   private:
    Mutex mutex{};
    File file{};
    uint64_t position{0}, valid{0};
    uint32_t capacity;
    bool recording{false};
};
uint32_t Engine::now_ms() const {
    return uint32_t(chTimeNow()) * (1000 / CH_FREQUENCY);
}
Engine::~Engine() {
    halt();
}
void Engine::decoded(const SubGhzDDataMessage& p) {
    if (s.mode != 8 || now_ms() - tuned_at < 150) return;
    if (p.sensorType == protocol_type && p.data == protocol_value && p.bits == protocol_bits && now_ms() - decode_tick < 1000) return;
    protocol_type = p.sensorType;
    decode_tick = now_ms();
    switch (p.sensorType) {
        case FPS_PRINCETON:
            protocol = "Princeton/Bytec";
            break;
        case FPS_CAME:
            protocol = "CAME";
            break;
        case FPS_CAMEATOMO:
            protocol = "CAME Atomo";
            break;
        case FPS_CAMETWEE:
            protocol = "CAME TWEE";
            break;
        case FPS_FAAC:
            protocol = "FAAC";
            break;
        case FPS_KEELOQ:
            protocol = "KeeLoq";
            break;
        case FPS_SECPLUSV1:
            protocol = "Security+ 1";
            break;
        case FPS_SECPLUSV2:
            protocol = "Security+ 2";
            break;
        case FPS_NICEFLO:
            protocol = "Nice FLO";
            break;
        case FPS_HOLTEKHT12X:
            protocol = "Holtek HT12x";
            break;
        default:
            protocol = "Unknown";
            break;
    }
    protocol_bits = p.bits;
    protocol_value = p.data;
    event(protocol);
}

void Engine::event(const char* text) {
    message = text;
    std::memmove(events[1], events[0], sizeof(events) - sizeof(events[0]));
    std::snprintf(events[0], sizeof(events[0]), "%lus %s", static_cast<unsigned long>(now_ms() / 1000), text);
    // Keep filesystem operations on the capture worker while it owns the
    // streaming file. History remains available in RAM during recording.
    if (s.log && !capture) {
        ensure_directory(u"LOGS");
        File f;
        if (!f.append(u"LOGS/REPEATER.TXT")) {
            if (f.size() < 262144) f.write_line(events[0]);
        }
    }
}
void Engine::configure() {
    if (s.rate != 25000 && s.rate != 50000 && s.rate != 100000 && s.rate != 250000 && s.rate != 500000) s.rate = 100000;
    s.delay = clip<uint32_t>(s.delay, 1, 30);
    s.attack = std::min<uint32_t>(s.attack, 1000U);
    s.release = clip<uint32_t>(s.release, 100, 3000);
    s.maximum = clip<uint32_t>(s.maximum, 1, 120);
    s.cooldown = clip<uint32_t>(s.cooldown, 1, 30);
    s.hysteresis = clip<uint32_t>(s.hysteresis, 1, 20);
    s.lna = std::min<uint32_t>(s.lna, 40U) / 8 * 8;
    s.vga = std::min<uint32_t>(s.vga, 62U) / 2 * 2;
    s.tx_gain = std::min<uint32_t>(s.tx_gain, 47U);
    s.mode = std::min<uint32_t>(s.mode, 8U);
    s.tx_mode = std::min<uint32_t>(s.tx_mode, 7U);
    s.audio_gain = clip<uint32_t>(s.audio_gain, 1, 20);
    s.repeats = clip<uint32_t>(s.repeats, 1, 10);
    s.gap = clip<uint32_t>(s.gap, 1, 30);
    s.prebuffer_ms = clip<uint32_t>(s.prebuffer_ms, s.attack + 250, 2000);
    s.sub_threshold = clip<uint32_t>(s.sub_threshold, 5, 90);
    s.dwell = clip<uint32_t>(s.dwell, s.attack + 300, 5000);
    s.hop_step = clip<uint32_t>(s.hop_step, 1000, 10000000);
    if (s.hop_end < s.rx || s.hop_end > 6000000000LL) s.hop_end = s.rx;
    control.config = {s.threshold, s.hysteresis, s.attack, s.release, s.maximum * 1000, s.delay, s.cooldown * 1000, false, s.wait_quiet, s.repeats, s.gap};
}
void Engine::start() {
    halt();
    configure();
    if (s.rx < 1000000 || s.rx > 6000000000LL || s.tx < 1000000 || s.tx > 6000000000LL) {
        fail("Invalid RX/TX frequency");
        return;
    }
    current_rx = s.rx;
    control.start(now_ms());
    listen();
}
void Engine::halt() {
    transmitter_model.disable();
    control.stop();
    calibrating = false;
    ready = true;
    replay.reset();
    ready = false;
    job.reset();
    stop_capture(false);
    receiver_model.disable();
    audio::output::stop();
    if (bb_loaded) {
        baseband::shutdown();
        bb_loaded = false;
    }
}
void Engine::fail(const char* text) {
    halt();
    control.fault();
    event(text);
}
void Engine::fail_file(const char* operation, File::Error error) {
    const auto reason = error.what();
    std::snprintf(file_error, sizeof(file_error), "%s %lu: %s", operation,
                  static_cast<unsigned long>(error.code()), reason.c_str());
    fail(file_error);
}
bool Engine::save_metadata() {
    const auto error = write_metadata_file(get_metadata_path(file_path), {received_frequency, playback_rate});
    if (error) {
        fail_file("Metadata", *error);
        return false;
    }
    auto side = file_path;
    side.replace_extension(u".RPT");
    File meta;
    const auto create_error = meta.create(side);
    if (create_error) {
        fail_file("Mode file", *create_error);
        return false;
    }
    const uint32_t values[2] = {0x32545052, playback_mode};
    const auto written = meta.write(values, sizeof(values));
    if (written.is_error()) {
        fail_file("Mode write", written.error());
        return false;
    }
    return true;
}
void Engine::listen() {
    transmitter_model.disable();
    receiver_model.disable();
    audio::output::stop();
    if (bb_loaded) baseband::shutdown();
    const bool raw = s.mode == IQ || s.mode == 8;
    auto tag = raw ? spi_flash::image_tag_capture : (s.mode == NFM || s.mode == FM) ? spi_flash::image_tag_nfm_audio
                                                : s.mode == WFM                     ? spi_flash::image_tag_wfm_audio
                                                                                    : spi_flash::image_tag_am_audio;
    baseband::run_image(tag);
    bb_loaded = true;
    shared_memory.request_m4_performance_counter = 0;
    if (raw) {
        receiver_model.set_modulation(ReceiverModel::Mode::Capture);
        const auto over = get_oversample_rate(s.rate);
        baseband::set_sample_rate(s.rate, over);
        receiver_model.set_sampling_rate(s.rate * toUType(over));
        receiver_model.set_baseband_bandwidth(ui::filter_bandwidth_for_sampling_rate(s.rate * toUType(over)));
        baseband::set_subghzd_config(0, s.mode == 8 ? s.rate : 0);
    } else {
        const auto mode = (s.mode == NFM || s.mode == FM) ? ReceiverModel::Mode::NarrowbandFMAudio : s.mode == WFM ? ReceiverModel::Mode::WidebandFMAudio
                                                                                                                   : ReceiverModel::Mode::AMAudio;
        receiver_model.set_modulation(mode);
        receiver_model.set_sampling_rate(3072000);
        receiver_model.set_baseband_bandwidth(1750000);
        receiver_model.set_squelch_level(0);
        receiver_model.set_am_squelch_level(0);
        receiver_model.set_nbfm_configuration(s.mode == FM ? 3 : 2);
        receiver_model.set_am_configuration(s.mode == USB ? 2 : s.mode == LSB ? 3
                                                                              : 1);
        receiver_model.set_wfm_configuration(0);
        audio::set_rate(s.mode == WFM ? audio::Rate::Hz_48000 : (s.mode == NFM || s.mode == FM) ? audio::Rate::Hz_24000
                                                                                                : audio::Rate::Hz_12000);
    }
    receiver_model.set_lna(s.lna);
    receiver_model.set_vga(s.vga);
    receiver_model.set_rf_amp(s.rx_amp);
    receiver_model.set_target_frequency(current_rx);
    if (raw && !arm_ring()) return;
    receiver_model.enable();
    if (!raw && s.monitor) {
        audio::output::start();
        receiver_model.set_headphone_volume(receiver_model.headphone_volume());
    }
    last_stats = tuned_at = now_ms();
    event(s.hop ? "Auto ON - receive hopping" : "Auto ON - listening");
}
bool Engine::arm_ring() {
    if (!ensure_directory(u"SAMPLES/REPEATER").ok()) {
        fail("SD directory unavailable");
        return false;
    }
    pending_path = next_filename_matching_pattern(u"SAMPLES/REPEATER/RPT_????.*");
    if (pending_path.empty()) {
        fail("Recording names exhausted");
        return false;
    }
    ring_temp = pending_path;
    ring_temp.replace_extension(u".TMP");
    // Match Capture's 16 KiB writes and three buffers. Wrap on a full write:
    // no partial-sector cache flush/seek in the middle of an incoming block.
    const auto requested = uint64_t(s.rate) * 4 * s.prebuffer_ms / 1000;
    const auto bytes = (requested + 16383) / 16384 * 16384;
    if (std::filesystem::space(u"SAMPLES/REPEATER").free < uint64_t(s.rate) * 8 * s.maximum + 2 * bytes + 1048576) {
        fail("Not enough free SD space");
        return false;
    }
    auto p = std::make_unique<RingWriter>(bytes);
    const auto create_error = p->create(ring_temp);
    if (create_error) {
        fail_file("Prebuffer", *create_error);
        return false;
    }
    ring = p.get();
    ring_recorded = false;
    capture = std::make_unique<CaptureThread>(std::move(p), 16384, 3, []() {}, [](File::Error e) {CaptureThreadDoneMessage m{e.code()};EventDispatcher::send_message(m); }, 2048);
    quality.reset(now_ms(), 0, 0);
    return true;
}
void Engine::retune(int64_t frequency) {
    receiver_model.disable();
    current_rx = frequency;
    receiver_model.set_target_frequency(frequency);
    if (s.mode == 8) baseband::set_subghzd_config(0, s.rate);
    if (ring && !ring->reset()) {
        fail("Cannot reset prebuffer");
        return;
    }
    if (capture) quality.reset(now_ms(), uint32_t(capture->state().baseband_bytes_received), uint32_t(capture->state().baseband_bytes_dropped));
    control.retuned(now_ms());
    tuned_at = last_stats = now_ms();
    receiver_model.enable();
}
void Engine::start_capture() {
    reject_current = false;
    dropped = 0;
    received_frequency = current_rx;
    playback_mode = s.mode;
    wav = s.mode != IQ && s.mode != 8;
    keep_current = s.keep || s.record_only;
    current_owned = true;
    sub_imported = false;
    if (!wav) {
        file_path = pending_path;
        file_path.replace_extension(u".C16");
        if (!ring || !ring->trigger()) {
            fail("Prebuffer trigger failed");
            return;
        }
        ring_recorded = true;
        playback_rate = s.rate;
        const auto prefix_ms = uint32_t(ring->prefix * 1000 / (uint64_t(s.rate) * 4));
        quality.begin(now_ms(), prefix_ms, uint32_t(capture->state().baseband_bytes_received), uint32_t(capture->state().baseband_bytes_dropped));
    } else {
        ensure_directory(u"SAMPLES/REPEATER");
        file_path = next_filename_matching_pattern(u"SAMPLES/REPEATER/RPT_????.*");
        if (file_path.empty()) {
            fail("Recording names exhausted");
            return;
        }
        file_path.replace_extension(u".WAV");
        playback_rate = s.mode == WFM ? 48000 : (s.mode == NFM || s.mode == FM) ? 24000
                                                                                : 12000;
        if (std::filesystem::space(u"SAMPLES/REPEATER").free < uint64_t(playback_rate) * 2 * s.maximum + 1048576) {
            fail("Not enough free SD space");
            return;
        }
        auto writer = std::make_unique<WAVFileWriter>();
        if (writer->create(file_path, playback_rate, "Auto Repeater")) {
            fail("Cannot create WAV");
            return;
        }
        capture = std::make_unique<CaptureThread>(std::move(writer), 4096, 4, []() {}, [](File::Error e) {CaptureThreadDoneMessage m{e.code()};EventDispatcher::send_message(m); });
        quality.reset(now_ms(), 0, 0);
        quality.begin(now_ms(), 0, 0, 0);
    }
    // Sidecars are written after stopping CaptureThread, just before file
    // processing. Do not contend with the real-time circular writer here.
    ++recorded;
    event("Recording signal");
}
void Engine::stop_capture(bool finish) {
    // Stop the producer/worker first. CaptureThread still owns its counters
    // and writer here, so the snapshot is stable and the ring is no longer
    // changing beneath the finalization job.
    if (capture) capture->stop();
    if (ring) {
        ring_prefix = ring->prefix;
        ring_end = ring->end;
    }
    dropped = finish && capture ? quality.percent(uint32_t(capture->state().baseband_bytes_received), uint32_t(capture->state().baseband_bytes_dropped)) : 0;
    capture.reset();
    ring = nullptr;
    if (!ring_temp.empty() && !ring_recorded) {
        delete_file(ring_temp);
        ring_temp = {};
    }
    if (!finish && ring_recorded) {
        event("Interrupted IQ kept as TMP");
        ring_recorded = false;
        ring_temp = {};
    }
}
void Engine::finish_capture() {
    stop_capture(true);
    receiver_model.disable();
    audio::output::stop();
    if (bb_loaded) {
        baseband::shutdown();
        bb_loaded = false;
    }
    reject_current = dropped != 0;
    if (reject_current) keep_current = true;
    if (!save_metadata()) return;
    if (ring_recorded) {
        if (!file_processor) {
            fail("File processor unavailable");
            return;
        }
        job = std::make_unique<FileJob>();
        job->kind = FileJob::FinishRing;
        job->input = ring_temp;
        job->output = file_path;
        job->prefix = ring_prefix;
        job->end = ring_end;
        job->frequency = received_frequency;
        job->export_sub = s.sub_save && keep_current && !reject_current;
        job->threshold_percent = s.sub_threshold;
        job->processor = file_processor;
        control.state = State::Processing;
        if (!job->start()) {
            fail("Not enough processing RAM");
            return;
        }
        ring_recorded = false;
        event("Finalizing buffered IQ");
        return;
    }
    if (!validate_file()) return;
    recording_ready();
}
bool Engine::validate_file() {
    wav = path_iequal(file_path.extension(), u".WAV");
    if (wav) {
        PCMReader reader;
        if (!reader.load(file_path) || reader.channels() != 1 || reader.bits_per_sample() != 16 || reader.data_size() > reader.file().size()) {
            fail("Need mono 16-bit WAV");
            return false;
        }
        playback_rate = reader.sample_rate();
        playback_ms = reader.ms_duration();
        if (playback_rate != 12000 && playback_rate != 24000 && playback_rate != 48000) {
            fail("WAV: use 12/24/48 kHz");
            return false;
        }
    } else {
        if (!path_iequal(file_path.extension(), u".C16")) {
            fail("Choose C16, WAV or SUB");
            return false;
        }
        auto meta = read_metadata_file(get_metadata_path(file_path));
        if (!meta || !::replay::select(meta->sample_rate).valid()) {
            fail("Missing IQ metadata");
            return false;
        }
        File f;
        if (f.open(file_path) || f.size() < 4 || f.size() % 4) {
            fail("Invalid IQ recording");
            return false;
        }
        playback_rate = meta->sample_rate;
        playback_ms = (f.size() / 4) * 1000 / playback_rate;
        received_frequency = meta->center_frequency;
    }
    if (!playback_ms || playback_ms > 123000) {
        fail("File is empty or too long");
        return false;
    }
    return true;
}
void Engine::send() {
    if (control.busy()) return;
    configure();
    if (file_path.empty()) {
        event("Open a file first");
        return;
    }
    if (!validate_file()) return;
    keep_current = true;
    current_owned = false;
    control.manual(now_ms());
    event("Manual send queued");
}
void Engine::start_tx() {
    const auto frequency = s.follow_rx ? received_frequency : s.tx;
    if (frequency < 1000000 || frequency > 6000000000LL) {
        fail("Invalid TX frequency");
        return;
    }
    std::unique_ptr<stream::Reader> reader{};
    if (wav) {
        auto p = std::make_unique<PCMReader>();
        if (!p->load(file_path)) {
            fail("Cannot open WAV");
            return;
        }
        reader = std::move(p);
    } else {
        auto p = std::make_unique<FileConvertReader>();
        if (p->open(file_path)) {
            fail("Cannot open IQ");
            return;
        }
        reader = std::move(p);
    }
    receiver_model.disable();
    audio::output::stop();
    transmitter_model.disable();
    if (bb_loaded) baseband::shutdown();
    if (wav) {
        baseband::run_image({'P', 'R', 'A', 'T'});
        bb_loaded = true;
        const auto mode = s.tx_mode ? s.tx_mode : (playback_mode && playback_mode < 8 ? playback_mode : NFM);
        const uint32_t deviation = mode == NFM ? 2500 : mode == FM ? 5000
                                                    : mode == WFM  ? 75000
                                                                   : 3000;
        baseband::set_audiotx_config(76800, deviation, s.audio_gain / 10.0f, 8, 16, 0, mode == AM, mode == DSB, mode == USB, mode == LSB);
        baseband::set_sample_rate(playback_rate);
        transmitter_model.set_sampling_rate(1536000);
        transmitter_model.set_baseband_bandwidth(1750000);
    } else {
        baseband::run_image(spi_flash::image_tag_replay);
        bb_loaded = true;
        const auto schedule = ::replay::select(playback_rate);
        baseband::set_sample_rate(playback_rate, static_cast<OversampleRate>(schedule.total_factor));
        transmitter_model.set_sampling_rate(schedule.output_rate);
        transmitter_model.set_baseband_bandwidth(playback_rate <= 500000 ? 1750000 : 2500000);
    }
    transmitter_model.set_target_frequency(frequency);
    transmitter_model.set_tx_gain(s.tx_gain);
    transmitter_model.set_rf_amp(s.tx_amp);
    ready = false;
    tx_started = now_ms();
    replay = std::make_unique<ReplayThread>(std::move(reader), 4096, 4, &ready, [](uint32_t c) {ReplayThreadDoneMessage m{c};EventDispatcher::send_message(m); }, true, false);
    transmitter_model.enable();
    event("Transmitting");
}
void Engine::tx_done(uint32_t code) {
    if (control.state != State::Transmitting) return;
    transmitter_model.disable();
    ready = true;
    replay.reset();
    ready = false;
    if (bb_loaded) {
        baseband::shutdown();
        bb_loaded = false;
    }
    if (code != ReplayThread::END_OF_FILE) {
        fail("TX interrupted; file kept");
        return;
    }
    ++transmitted;
    if (control.final_repeat() && current_owned && !keep_current) {
        if (!delete_file(file_path).ok()) {
            fail("TX OK; delete failed");
            return;
        }
        delete_file(get_metadata_path(file_path));
        auto side = file_path;
        side.replace_extension(u".RPT");
        delete_file(side);
        file_path = {};
        current_owned = false;
    }
    control.transmitted(now_ms());
    event(control.automatic ? "Auto ON - TX complete" : "Manual TX complete");
}
void Engine::capture_done(uint32_t error) {
    if (capture && error) fail_file("SD write", File::Error{error});
}
void Engine::perform(Action a) {
    switch (a) {
        case Action::Capture:
            start_capture();
            break;
        case Action::FinishCapture:
            finish_capture();
            break;
        case Action::Transmit:
            start_tx();
            break;
        case Action::Listen:
            listen();
            break;
        default:
            break;
    }
}
void Engine::statistics(const ChannelStatistics& stats) {
    if (control.state != State::Listening && control.state != State::Recording && control.state != State::Quiet) return;
    last_stats = now_ms();
    if (capture) quality.note(last_stats, uint32_t(capture->state().baseband_bytes_received), uint32_t(capture->state().baseband_bytes_dropped));
    if (last_stats - tuned_at < 150) return;
    level = stats.max_db;
    if (calibrating) {
        noise_sum += level;
        if (++calibration_count >= 20) {
            s.threshold = clip<int32_t>(noise_sum / 20 + 10, -100, 0);
            halt();
            event("Trigger set 10dB above noise");
        }
        return;
    }
    perform(control.sample(last_stats, level));
}
void Engine::process_finished() {
    if (!job || !job->done) return;
    const bool ok = job->ok;
    const auto kind = job->kind;
    const auto* error = job->error;
    if (kind == FileJob::ImportSUB) {
        received_frequency = job->frequency;
        file_path = job->output;
        playback_mode = IQ;
        sub_imported = true;
        s.tx = received_frequency;
    }
    job.reset();
    if (!ok) {
        fail(error);
        return;
    }
    if (kind == FileJob::ImportSUB) {
        control.stop();
        event("Flipper file ready - press Send");
        return;
    }
    ring_temp = {};
    if (!validate_file()) return;
    recording_ready(error);
}
void Engine::recording_ready(const char* warning) {
    if (reject_current) {
        ++skipped;
        control.recorded_only(now_ms());
        event("RX gap: saved; auto rearming");
    } else if (s.record_only) {
        control.recorded_only(now_ms());
        event("Saved - record only");
    } else {
        control.state = State::Delay;
        control.entered = now_ms();
        event(warning ? warning : "Recording ready");
    }
}
void Engine::tick() {
    process_finished();
    if (!control.busy()) return;
    const auto now = now_ms();
    if (control.state == State::Processing) {
        if (job && !job->thread && !job->done && !job->start()) fail("Not enough processing RAM");
        return;
    }
    if ((control.state == State::Listening || control.state == State::Recording || control.state == State::Quiet) && now - last_stats > 3000) {
        fail("RX statistics timed out");
        return;
    }
    if (s.hop && !calibrating && control.state == State::Listening && now - tuned_at >= s.dwell) {
        const auto next = current_rx + s.hop_step;
        retune(next > s.hop_end ? s.rx : next);
    }
    if (control.state == State::Transmitting && now - tx_started > playback_ms + 5000) {
        fail("TX timeout; file kept");
        return;
    }
    perform(control.tick(now));
}
void Engine::select_file(const std::filesystem::path& path) {
    halt();
    file_path = path;
    current_owned = false;
    keep_current = true;
    if (path_iequal(path.extension(), u".SUB")) {
        if (!file_processor) {
            fail("SUB converter unavailable");
            return;
        }
        ensure_directory(u"SAMPLES/REPEATER");
        job = std::make_unique<FileJob>();
        job->kind = FileJob::ImportSUB;
        job->input = path;
        job->output = next_filename_matching_pattern(u"SAMPLES/REPEATER/SUB_????.*");
        job->output.replace_extension(u".C16");
        job->processor = file_processor;
        control.state = State::Processing;
        event("Importing Flipper SUB");
        return;
    }
    playback_mode = s.mode;
    auto meta = read_metadata_file(get_metadata_path(path));
    if (meta) received_frequency = meta->center_frequency;
    auto side = path;
    side.replace_extension(u".RPT");
    File f;
    uint32_t values[2]{};
    if (!f.open(side)) {
        auto r = f.read(values, sizeof(values));
        if (r.is_ok() && r.value() == sizeof(values) && values[0] == 0x32545052) playback_mode = values[1];
    }
    if (validate_file()) event("File ready - press Send");
}
void Engine::calibrate() {
    start();
    if (!control.busy()) return;
    calibrating = true;
    calibration_count = 0;
    noise_sum = 0;
    event("Measuring noise (2 seconds)");
}
const char* Engine::state_text() {
    const char* text = "STOPPED";
    switch (control.state) {
        case State::Listening:
            text = calibrating ? "MEASURING NOISE" : s.hop ? "AUTO ON - HOPPING"
                                                           : "AUTO ON - LISTENING";
            break;
        case State::Recording:
            text = "RECORDING";
            break;
        case State::Processing:
            std::snprintf(state_buffer, sizeof(state_buffer), "PROCESSING %lu%%", static_cast<unsigned long>(job ? job->percent.load() : 0));
            return state_buffer;
        case State::Delay:
        case State::Gap:
            std::snprintf(state_buffer, sizeof(state_buffer), "TX IN %lus (%lu/%lu)", static_cast<unsigned long>(control.remaining_s(now_ms())), static_cast<unsigned long>(control.completed + 1), static_cast<unsigned long>(s.repeats));
            return state_buffer;
        case State::Transmitting:
            text = "TRANSMITTING";
            break;
        case State::Cooldown:
            text = "AUTO ON - COOLDOWN";
            break;
        case State::Quiet:
            text = "AUTO ON - WAIT QUIET";
            break;
        case State::Fault:
            text = "STOPPED - CHECK MESSAGE";
            break;
        default:
            break;
    }
    return text;
}
}  // namespace auto_repeater

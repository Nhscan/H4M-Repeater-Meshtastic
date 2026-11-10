// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "repeater_control.hpp"
#include "capture_thread.hpp"
#include "replay_thread.hpp"
#include "rf_path.hpp"
#include "file.hpp"
#include "repeater_jobs.hpp"
#include "repeater_quality.hpp"

namespace auto_repeater {
void process_repeater_files(FileJob& job);
enum Mode : uint32_t { IQ = 0,
                       NFM,
                       FM,
                       AM,
                       USB,
                       LSB,
                       WFM,
                       DSB };
struct Settings {
    int64_t rx{145500000}, tx{145500000};
    int32_t threshold{-45};
    uint32_t rate{100000}, delay{3}, attack{0}, release{500};
    uint32_t maximum{30}, cooldown{1}, hysteresis{6};
    uint32_t lna{24}, vga{24}, tx_gain{0};
    bool keep{true}, once{false};
    uint32_t mode{IQ}, tx_mode{0}, repeats{1}, gap{1}, audio_gain{10};
    bool tx_amp{false}, rx_amp{false}, monitor{false}, wait_quiet{false};
    bool record_only{false}, log{true}, sub_save{true}, hop{false}, follow_rx{false};
    uint32_t prebuffer_ms{500}, sub_threshold{35}, hop_step{25000}, dwell{1000};
    int64_t hop_end{145500000};
};
class Engine {
   public:
    explicit Engine(Settings& settings)
        : s{settings} {}
    ~Engine();
    Control control{};
    bool ready{false};
    void (*file_processor)(FileJob&){nullptr};
    int64_t current_rx{145500000}, received_frequency{145500000};
    uint32_t recorded{0}, transmitted{0}, last_stats{0};
    int32_t level{-100};
    std::filesystem::path file_path{};
    const char* message{"Ready"};
    void start();
    void send();
    void halt();
    void stop() {
        halt();
        message = "Stopped; recording kept";
    }
    void fail(const char* text);
    void tick();
    void statistics(const ChannelStatistics& stats);
    void tx_done(uint32_t code);
    void capture_done(uint32_t error);
    void select_file(const std::filesystem::path& path);
    void calibrate();
    uint32_t now_ms() const;
    const char* state_text();
    const char* history(size_t index) const { return index < 8 ? events[index] : ""; }
    uint32_t decode_tick{0};
    uint8_t protocol_type{0};
    uint32_t dropped{0};
    uint32_t skipped{0};
    const char* protocol{"Unknown"};
    uint16_t protocol_bits{0};
    uint64_t protocol_value{0};
    void decoded(const SubGhzDDataMessage& packet);

   private:
    Settings& s;
    bool bb_loaded{false}, current_owned{false}, keep_current{true}, wav{false};
    bool calibrating{false};
    bool reject_current{false};
    CaptureQuality quality{};
    std::unique_ptr<FileJob> job{};
    std::filesystem::path ring_temp{}, pending_path{};
    class RingWriter* ring{nullptr};
    bool ring_recorded{false};
    uint64_t ring_prefix{0}, ring_end{0};
    uint32_t tuned_at{0};
    bool sub_imported{false};
    uint32_t calibration_count{0};
    int32_t noise_sum{0};
    uint32_t playback_rate{100000}, playback_ms{0}, playback_mode{IQ}, tx_started{0};
    std::unique_ptr<CaptureThread> capture{};
    std::unique_ptr<ReplayThread> replay{};
    char state_buffer[40]{};
    char file_error[64]{};
    char events[8][40]{};
    void configure();
    void listen();
    void perform(Action action);
    void start_capture();
    void finish_capture();
    void start_tx();
    bool arm_ring();
    void stop_capture(bool finish);
    void process_finished();
    void recording_ready(const char* warning = nullptr);
    void queue_processing();
    void retune(int64_t frequency);
    bool validate_file();
    bool save_metadata();
    void fail_file(const char* operation, File::Error error);
    void event(const char* text);
};
}  // namespace auto_repeater

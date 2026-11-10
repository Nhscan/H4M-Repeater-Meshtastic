// SPDX-License-Identifier: GPL-2.0-or-later
// Recorded PCM -> the same AM/FM/SSB modulators used by Mayhem Mic TX.
#include "baseband_processor.hpp"
#include "baseband_thread.hpp"
#include "stream_output.hpp"
#include "dsp_modulate.hpp"
#include "event_m4.hpp"
#include "portapack_shared_memory.hpp"
#include <cstring>

namespace {
struct Lock {
    Mutex& m;
    Lock(Mutex& v)
        : m(v) { chMtxLock(&m); }
    ~Lock() { chMtxUnlock(); }
};
class RecordedTX : public BasebandProcessor {
   public:
    RecordedTX() {
        chMtxInit(&mutex);
        baseband_thread.start();
    }
    void execute(const buffer_c8_t& buffer) override {
        Lock guard{mutex};
        if (!configured || !stream || !modulator) {
            for (size_t i = 0; i < buffer.count; i++) buffer.p[i] = {0, 0};
            return;
        }
        // The three RX audio rates are exact ratios of 24 kHz.
        for (size_t i = 0; i < buffer.count / 64; i++) {
            if (source_rate == 12000) {
                if (!(audio_index++ & 1)) stream->read(&held, sizeof(held));
                samples[i] = held;
            } else if (source_rate == 48000) {
                int16_t pair[2]{};
                stream->read(pair, sizeof(pair));
                samples[i] = (int32_t(pair[0]) + pair[1]) / 2;
            } else {
                samples[i] = 0;
                stream->read(&samples[i], sizeof(int16_t));
            }
        }
        buffer_s16_t audio{samples, buffer.count / 64, 24000};
        modulator->execute(audio, buffer, configured, beep_index, beep_timer, progress, level, power_count, divider);
    }
    void on_message(const Message* message) override {
        Lock guard{mutex};
        switch (message->id) {
            case Message::ID::AudioTXConfig: {
                const auto& c = *static_cast<const AudioTXConfigMessage*>(message);
                configured = false;
                modulator.reset();
                if (c.usb_enabled || c.lsb_enabled) {
                    auto* m = new dsp::modulate::SSB();
                    m->set_fs_div_factor(3000);
                    m->set_mode(c.usb_enabled ? dsp::modulate::Mode::USB : dsp::modulate::Mode::LSB);
                    modulator.reset(m);
                } else if (c.am_enabled || c.dsb_enabled) {
                    auto* m = new dsp::modulate::AM();
                    m->set_mode(c.am_enabled ? dsp::modulate::Mode::AM : dsp::modulate::Mode::DSB);
                    modulator.reset(m);
                } else {
                    auto* m = new dsp::modulate::FM();
                    m->set_fm_delta(c.deviation_hz * (0xFFFFFFUL / 1536000));
                    modulator.reset(m);
                }
                modulator->set_over(64);
                modulator->set_gain_shiftbits_vumeter_beep(c.audio_gain, 8, false);
                break;
            }
            case Message::ID::SampleRateConfig:
                source_rate = static_cast<const SampleRateConfigMessage*>(message)->sample_rate;
                audio_index = 0;
                held = 0;
                break;
            case Message::ID::ReplayConfig: {
                configured = false;
                const auto* c = static_cast<const ReplayConfigMessage*>(message)->config;
                if (c) {
                    stream = std::make_unique<StreamOutput>(const_cast<ReplayConfig*>(c));
                    RequestSignalMessage ready{RequestSignalMessage::Signal::FillRequest};
                    shared_memory.application_queue.push(ready);
                } else
                    stream.reset();
                break;
            }
            case Message::ID::FIFOData:
                configured = true;
                break;
            default:
                break;
        }
    }

   private:
    Mutex mutex{};
    bool configured{false};
    uint32_t source_rate{24000}, audio_index{0};
    int16_t held{0}, samples[32]{};
    std::unique_ptr<StreamOutput> stream{};
    std::unique_ptr<dsp::modulate::Modulator> modulator{};
    uint32_t beep_index{0}, beep_timer{0}, power_count{0}, divider{76800};
    TXProgressMessage progress{};
    AudioLevelReportMessage level{};
    BasebandThread baseband_thread{1536000, this, baseband::Direction::Transmit, false};
};
}  // namespace
int main() {
    EventDispatcher dispatcher{std::make_unique<RecordedTX>()};
    dispatcher.run();
}

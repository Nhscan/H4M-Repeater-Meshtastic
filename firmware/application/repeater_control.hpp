// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
namespace auto_repeater {
enum class State { Stopped,
                   Listening,
                   Recording,
                   Processing,
                   Delay,
                   Transmitting,
                   Gap,
                   Cooldown,
                   Quiet,
                   Fault };
enum class Action { None,
                    Capture,
                    FinishCapture,
                    Transmit,
                    Listen };
struct Config {
    int32_t threshold{-45};
    uint32_t hysteresis{6}, attack_ms{0}, release_ms{500}, max_record_ms{30000};
    uint32_t delay_s{3}, cooldown_ms{1000};
    bool once{false}, wait_quiet{false};
    uint32_t repeats{1}, gap_s{1};
};
class Control {
   public:
    Config config{};
    State state{State::Stopped};
    bool automatic{false};
    uint32_t entered{0}, completed{0};
    void start(uint32_t now) {
        sanitize();
        completed = 0;
        automatic = true;
        enter(State::Listening, now);
    }
    void manual(uint32_t now) {
        sanitize();
        completed = 0;
        automatic = false;
        enter(State::Delay, now);
    }
    void stop() {
        automatic = false;
        state = State::Stopped;
        high = false;
    }
    void fault() {
        stop();
        state = State::Fault;
    }
    bool busy() const { return state != State::Stopped && state != State::Fault; }
    void retuned(uint32_t now) { enter(State::Listening, now); }
    Action sample(uint32_t now, int32_t db) {
        if (state == State::Listening) {
            if (db >= config.threshold) {
                if (!high) {
                    high = true;
                    high_since = now;
                }
                if (now - high_since >= config.attack_ms) {
                    completed = 0;
                    last_active = now;
                    enter(State::Recording, now);
                    return Action::Capture;
                }
            } else
                high = false;
        } else if (state == State::Recording) {
            // End silence is measured below the selected trigger. Applying the
            // rearm hysteresis here can put the release level below the noise
            // floor and hold every short burst until the maximum-time limit.
            if (db >= config.threshold) last_active = now;
            if (now - last_active >= config.release_ms) {
                enter(State::Delay, now);
                return Action::FinishCapture;
            }
        } else if (state == State::Quiet) {
            if (db < config.threshold - int32_t(config.hysteresis)) {
                if (!high) {
                    high = true;
                    high_since = now;
                }
                if (now - high_since >= config.release_ms) enter(State::Listening, now);
            } else
                high = false;
        }
        return Action::None;
    }
    Action tick(uint32_t now) {
        if (state == State::Recording && now - entered >= config.max_record_ms) {
            enter(State::Delay, now);
            return Action::FinishCapture;
        }
        if ((state == State::Delay && now - entered >= config.delay_s * 1000) || (state == State::Gap && now - entered >= config.gap_s * 1000)) {
            enter(State::Transmitting, now);
            return Action::Transmit;
        }
        if (state == State::Cooldown && now - entered >= config.cooldown_ms) {
            enter(config.wait_quiet ? State::Quiet : State::Listening, now);
            return Action::Listen;
        }
        return Action::None;
    }
    void transmitted(uint32_t now) {
        if (state != State::Transmitting) return;
        ++completed;
        if (completed < config.repeats) {
            enter(State::Gap, now);
            return;
        }
        if (!automatic || config.once)
            stop();
        else
            enter(State::Cooldown, now);
    }
    void recorded_only(uint32_t now) {
        completed = 0;
        enter(State::Cooldown, now);
    }
    bool final_repeat() const { return completed + 1 >= config.repeats; }
    uint32_t remaining_s(uint32_t now) const {
        const auto total = (state == State::Gap ? config.gap_s : config.delay_s) * 1000;
        return now - entered >= total ? 0 : (total - (now - entered) + 999) / 1000;
    }

   private:
    bool high{false};
    uint32_t high_since{0}, last_active{0};
    void enter(State next, uint32_t now) {
        state = next;
        entered = now;
        high = false;
    }
    void sanitize() {
        if (config.delay_s < 1) config.delay_s = 1;
        if (config.delay_s > 30) config.delay_s = 30;
        if (config.repeats < 1) config.repeats = 1;
        if (config.repeats > 10) config.repeats = 10;
        if (config.gap_s < 1) config.gap_s = 1;
        if (config.gap_s > 30) config.gap_s = 30;
        if (config.release_ms < 100) config.release_ms = 100;
        if (config.max_record_ms < 1000) config.max_record_ms = 1000;
    }
};
}  // namespace auto_repeater

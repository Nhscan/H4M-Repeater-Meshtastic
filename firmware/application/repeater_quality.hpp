// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>

namespace auto_repeater {
// Capture counters run for the whole listening session. Keep a short history
// so only losses in the retained prebuffer and current event affect its quality.
// Low 32-bit byte counters wrap safely: one event is much shorter than 4 GiB.
class CaptureQuality {
   public:
    struct Sample {
        uint32_t tick{0}, received{0}, lost{0};
    };
    void reset(uint32_t now, uint32_t received, uint32_t lost) {
        count = next = 0;
        note(now, received, lost);
        baseline = history[0];
    }
    void note(uint32_t now, uint32_t received, uint32_t lost) {
        if (count && history[(next + capacity - 1) % capacity].tick == now) {
            history[(next + capacity - 1) % capacity] = {now, received, lost};
            return;
        }
        history[next] = {now, received, lost};
        next = (next + 1) % capacity;
        if (count < capacity) ++count;
    }
    void begin(uint32_t now, uint32_t prebuffer_ms, uint32_t received, uint32_t lost) {
        note(now, received, lost);
        for (uint32_t n = 0; n < count; ++n) {
            baseline = history[(next + capacity - 1 - n) % capacity];
            if (now - baseline.tick >= prebuffer_ms) break;
        }
    }
    uint32_t lost_since_begin(uint32_t lost) const { return lost - baseline.lost; }
    uint32_t percent(uint32_t received, uint32_t lost) const {
        const uint32_t missing = lost_since_begin(lost);
        if (!missing) return 0;
        const uint32_t total = received - baseline.received;
        const uint32_t result = total ? uint64_t(missing) * 100 / total : 100;
        return result ? (result > 100 ? 100 : result) : 1;
    }

   private:
    static constexpr uint32_t capacity{32};
    Sample history[capacity]{};
    Sample baseline{};
    uint32_t count{0}, next{0};
};
}  // namespace auto_repeater

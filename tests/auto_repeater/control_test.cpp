#include "../../firmware/application/repeater_control.hpp"
#include <cassert>
#include <cstdio>
using namespace auto_repeater;
int main() {
    Control c;
    c.config.attack_ms = 100;
    c.config.delay_s = 1;
    c.start(0);
    c.sample(10, -20);
    c.sample(50, -80);
    c.sample(100, -20);
    assert(c.sample(200, -20) == Action::Capture);
    c.sample(300, -44);
    assert(c.sample(799, -90) == Action::None);
    assert(c.sample(800, -90) == Action::FinishCapture);
    assert(c.tick(1799) == Action::None);
    assert(c.tick(1800) == Action::Transmit);
    c.transmitted(2000);
    assert(c.automatic);
    assert(c.state == State::Cooldown);
    assert(c.tick(3000) == Action::Listen);
    assert(c.state == State::Listening);
    assert(c.automatic);
    // Reproduce the reported failure: repeated automatic cycles must stay armed.
    uint32_t t = 4000;
    for (int i = 0; i < 1000; i++) {
        c.sample(t, -20);
        assert(c.sample(t + 100, -20) == Action::Capture);
        assert(c.sample(t + 600, -90) == Action::FinishCapture);
        assert(c.tick(t + 1600) == Action::Transmit);
        c.transmitted(t + 1800);
        assert(c.tick(t + 2800) == Action::Listen);
        assert(c.automatic && c.state == State::Listening);
        t += 3000;
    }
    c.config.wait_quiet = true;
    c.start(0);
    c.sample(0, 0);
    c.sample(100, 0);
    c.sample(600, -90);
    c.tick(1600);
    c.transmitted(1800);
    c.tick(2800);
    assert(c.state == State::Quiet);
    c.sample(2900, -90);
    c.sample(3400, -90);
    assert(c.state == State::Listening);
    c.config.repeats = 3;
    c.config.gap_s = 2;
    c.manual(0);
    c.tick(1000);
    assert(!c.final_repeat());
    c.transmitted(1500);
    assert(c.state == State::Gap);
    assert(c.tick(3499) == Action::None);
    assert(c.tick(3500) == Action::Transmit);
    c.transmitted(4000);
    assert(c.tick(6000) == Action::Transmit);
    assert(c.final_repeat());
    c.transmitted(6500);
    assert(!c.busy());
    c.config.delay_s = 30;
    c.manual(0);
    assert(c.tick(29999) == Action::None);
    assert(c.tick(30000) == Action::Transmit);
    c.manual(0);
    c.stop();
    assert(c.tick(90000) == Action::None);
    c.config.repeats = 1;
    c.config.max_record_ms = 1000;
    c.start(0);
    c.sample(0, 0);
    c.sample(100, 0);
    assert(c.tick(1100) == Action::FinishCapture);
    c.config.delay_s = 1;
    c.manual(0xffffff00);
    assert(c.tick(0x2e7) == Action::None);
    assert(c.tick(0x2e8) == Action::Transmit);
    c.config.delay_s = 0;
    c.manual(0);
    assert(c.config.delay_s == 1);
    c.config.delay_s = 100;
    c.manual(0);
    assert(c.config.delay_s == 30);
    c.fault();
    assert(!c.busy());
    // Video regression: trigger -44, background -47, rearm margin 6. The
    // background must not hold a brief event open for the 30-second maximum.
    c.config.threshold = -44;
    c.config.hysteresis = 6;
    c.config.attack_ms = 0;
    c.config.release_ms = 500;
    c.config.wait_quiet = false;
    c.config.delay_s = 3;
    c.start(0);
    assert(c.sample(0, -47) == Action::None);
    assert(c.sample(100, -20) == Action::Capture);
    assert(c.sample(599, -47) == Action::None);
    assert(c.sample(600, -47) == Action::FinishCapture);
    assert(c.tick(3599) == Action::None);
    assert(c.tick(3600) == Action::Transmit);
    c.transmitted(3700);
    assert(c.tick(4700) == Action::Listen && c.automatic);
    // A damaged recording follows the record-only cooldown path and stays
    // automatic. It must not be sent during that cooldown.
    c.sample(4800, -20);
    assert(c.sample(5300, -47) == Action::FinishCapture);
    c.recorded_only(5400);
    assert(c.tick(6399) == Action::None);
    assert(c.tick(6400) == Action::Listen && c.automatic);
    std::puts("PASS: 1000 automatic cycles, quiet option, repeats/gaps, 1/30s delay, cancel, trigger/hysteresis/release, max time and timer wrap");
}

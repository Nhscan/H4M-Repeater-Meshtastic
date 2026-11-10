#include "../../firmware/application/repeater_quality.hpp"
#include "../../firmware/application/repeater_ui_helpers.hpp"
#include <cassert>
#include <cstdio>
#include <string>
using namespace auto_repeater;
struct Text {
    std::string value;
    unsigned paints{0};
    std::string get() const { return value; }
    void set(std::string_view s) { value = s; ++paints; }
};
struct Button {
    std::string value;
    unsigned paints{0};
    std::string text() const { return value; }
    void set_text(const char* s) { value = s; ++paints; }
};
int main() {
    CaptureQuality q;
    q.reset(0, 0, 0);
    q.note(100, 40000, 100); // A drop while first opening the app.
    for (unsigned ms = 200; ms <= 5000; ms += 100) q.note(ms, ms * 400, 100);
    q.begin(5000, 500, 2000000, 100);
    assert(q.lost_since_begin(100) == 0);
    assert(q.percent(2200000, 100) == 0); // Idle loss must not reject this event.
    assert(q.lost_since_begin(180) == 80);
    assert(q.percent(2200000, 180) == 1); // Even a small real gap is detected.
    q.note(5100, 2040000, 180);
    q.begin(5200, 500, 2080000, 180);
    assert(q.lost_since_begin(180) == 80); // A loss IN retained prebuffer counts.
    q.reset(5300, 2120000, 180); // Retune: history from the old channel is gone.
    q.begin(5400, 500, 2160000, 180);
    assert(q.percent(2200000, 180) == 0);
    q.reset(0xffffff00, 0xffffff00, 0xfffffff0);
    q.begin(0xffffff00, 0, 0xffffff00, 0xfffffff0);
    assert(q.lost_since_begin(0x10) == 32); // Byte-counter wrap.
    assert(q.percent(0x300, 0x10) == 3);
    q.reset(0xffffff00, 10, 1);
    q.note(0xffffff80, 20, 1);
    q.begin(0x100, 300, 40, 2);
    assert(q.lost_since_begin(2) == 1); // Tick wrap and prebuffer lookup.
    RefreshGate gate;
    Text text;
    Button button;
    unsigned refreshes = 0;
    for (unsigned ms = 0; ms < 1000; ms += 16) {
        if (!gate.ready(ms)) continue;
        ++refreshes;
        update_text(text, "Listening");
        update_caption(button, "START AUTO");
    }
    assert(refreshes == 5);
    assert(text.paints == 1 && button.paints == 1); // Stable display, no repaint loop.
    assert(gate.ready(1000, true));
    update_caption(button, "STOP AUTO");
    assert(button.paints == 2 && button.value == "STOP AUTO");
    RefreshGate wrap;
    assert(wrap.ready(0xffffff80));
    assert(!wrap.ready(0x20));
    assert(wrap.ready(0x80));
    std::puts("PASS: idle/retained-prebuffer/event drop accounting, retunes/counter/time wrap, 5Hz refresh and unchanged-label repaint suppression");
}

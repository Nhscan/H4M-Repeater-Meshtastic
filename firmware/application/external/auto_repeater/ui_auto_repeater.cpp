// SPDX-License-Identifier: GPL-2.0-or-later
#include "ui_auto_repeater.hpp"
#include "ui_fileman.hpp"
#include "baseband_api.hpp"
#include "string_format.hpp"
#include "portapack.hpp"
using namespace portapack;
namespace ui::external_app::auto_repeater {
HistoryView::HistoryView(NavigationView& nav, Engine& e) {
    for (size_t i = 0; i < 8; i++) {
        rows[i]->set_parent_rect({UI_POS_X(1), UI_POS_Y(i * 2), UI_POS_WIDTH(28), UI_POS_HEIGHT(1)});
        rows[i]->set(e.history(i));
        add_child(rows[i]);
    }
    add_child(&done);
    done.on_select = [&nav](Button&) { nav.pop(); };
}
RepeaterSettingsView::RepeaterSettingsView(NavigationView& nav, Settings& settings, Engine& engine)
    : s{settings}, e{engine}, nav_{nav} {
    add_children({&page, &tx_mode, &rate, &end_frequency, &note, &done, &calibrate, &history});
    for (size_t i = 0; i < 8; i++) {
        labels[i]->set_parent_rect({UI_POS_X(1), UI_POS_Y(i + 2), UI_POS_WIDTH(21), UI_POS_HEIGHT(1)});
        add_child(labels[i]);
        add_child(numbers[i]);
        numbers[i]->on_change = [this, i](int32_t v) {if(!updating&&values[i])*values[i]=v; };
    }
    for (size_t i = 0; i < 3; i++) {
        add_child(checks[i]);
        checks[i]->on_select = [this, i](Checkbox&, bool v) {if(!updating&&flags[i])*flags[i]=v; };
    }
    page.on_change = [this](size_t, uint32_t p) { show_page(p); };
    tx_mode.set_by_value(s.tx_mode);
    tx_mode.on_change = [this](size_t, uint32_t v) { s.tx_mode = v; };
    rate.set_by_value(s.rate);
    rate.on_change = [this](size_t, uint32_t v) { s.rate = v; };
    end_frequency.set_value(s.hop_end);
    end_frequency.on_change = [this](rf::Frequency f) { s.hop_end = f; };
    end_frequency.on_edit = [this]() {auto* v=nav_.push<FrequencyKeypadView>(s.hop_end);v->on_changed=[this](rf::Frequency f){end_frequency.set_value(f);}; };
    done.on_select = [&nav](Button&) { nav.pop(); };
    calibrate.on_select = [this](Button&) {auto* engine=&e;auto* nav=&nav_;nav->pop();engine->calibrate(); };
    history.on_select = [this](Button&) { nav_.push<HistoryView>(e); };
    show_page(0);
}
void RepeaterSettingsView::row(size_t i, const char* label, uint32_t& value, int32_t min, int32_t max, int32_t step) {
    values[i] = &value;
    labels[i]->set(label);
    labels[i]->hidden(false);
    numbers[i]->hidden(false);
    numbers[i]->set_range(min, max);
    numbers[i]->set_step(step);
    numbers[i]->set_value(value, false);
}
void RepeaterSettingsView::flag(size_t i, const char* label, bool& value) {
    flags[i] = &value;
    checks[i]->set_text(label);
    checks[i]->set_value(value);
    checks[i]->hidden(false);
}
void RepeaterSettingsView::show_page(uint32_t p) {
    updating = true;
    for (size_t i = 0; i < 8; i++) {
        values[i] = nullptr;
        labels[i]->hidden(true);
        numbers[i]->hidden(true);
    }
    for (size_t i = 0; i < 3; i++) {
        flags[i] = nullptr;
        checks[i]->hidden(true);
    }
    tx_mode.hidden(p != 0);
    rate.hidden(p != 2);
    end_frequency.hidden(p != 3);
    if (p == 0) {
        row(0, "RX LNA dB", s.lna, 0, 40, 8);
        row(1, "RX VGA dB", s.vga, 0, 62, 2);
        row(2, "TX gain dB", s.tx_gain, 0, 47);
        row(3, "Audio gain x0.1", s.audio_gain, 1, 20);
        labels[6]->set("Voice TX mode");
        labels[6]->hidden(false);
        flag(0, "RX amplifier", s.rx_amp);
        flag(1, "Hear received voice", s.monitor);
        flag(2, "TX follows received freq", s.follow_rx);
        note.set("IQ preserves original mode");
    } else if (p == 1) {
        row(0, "Trigger hold ms", s.attack, 0, 1000, 100);
        row(1, "End silence ms", s.release, 100, 3000, 100);
        row(2, "Rearm margin dB", s.hysteresis, 1, 20);
        row(3, "Max record sec", s.maximum, 1, 120);
        row(4, "Cooldown sec", s.cooldown, 1, 30);
        row(5, "Repeat count", s.repeats, 1, 10);
        row(6, "Repeat gap sec", s.gap, 1, 30);
        flag(0, "Wait quiet before rearm", s.wait_quiet);
        note.set("Auto stays ON until STOP");
    } else if (p == 2) {
        labels[0]->set("IQ rate");
        labels[0]->hidden(false);
        row(1, "IQ prebuffer ms", s.prebuffer_ms, 250, 2000, 250);
        row(2, "SUB OOK level %", s.sub_threshold, 5, 90, 5);
        flag(0, "Save OOK .SUB with IQ", s.sub_save);
        flag(1, "Record only (no auto TX)", s.record_only);
        flag(2, "Write SD event log", s.log);
        note.set("WAV and arbitrary RF != SUB");
    } else {
        labels[0]->set("End:");
        labels[0]->hidden(false);
        row(1, "Step Hz", s.hop_step, 1000, 1000000, 1000);
        row(2, "Dwell ms", s.dwell, 300, 5000, 100);
        flag(0, "Enable receive hopping", s.hop);
        flag(1, "TX follows received freq", s.follow_rx);
        note.set("Start is main RX frequency");
    }
    updating = false;
}
AutoRepeaterView::AutoRepeaterView(NavigationView& nav)
    : nav_{nav} {
    perf = shared_memory.request_m4_performance_counter;
    e.file_processor = ::auto_repeater::process_repeater_files;
    add_children({&labels, &rx, &tx, &threshold, &level, &meter, &mode, &delay, &keep, &amp, &protocol, &state, &detail, &counts, &decoded, &start, &settings, &open, &send});
    rx.set_value(s.rx);
    tx.set_value(s.tx);
    threshold.set_value(s.threshold);
    delay.set_value(s.delay);
    keep.set_value(s.keep);
    amp.set_value(s.tx_amp);
    mode.set_by_value(s.mode);
    rx.on_change = [this](rf::Frequency f) {if(e.control.busy()){if(f!=s.rx)rx.set_value(s.rx);}else s.rx=f; };
    tx.on_change = [this](rf::Frequency f) {if(e.control.busy()){if(f!=s.tx)tx.set_value(s.tx);}else s.tx=f; };
    rx.on_edit = [this]() {if(!e.control.busy())edit(rx); };
    tx.on_edit = [this]() {if(!e.control.busy())edit(tx); };
    threshold.on_change = [this](int32_t v) {if(e.control.busy())threshold.set_value(s.threshold,false);else s.threshold=v; };
    delay.on_change = [this](int32_t v) {if(e.control.busy())delay.set_value(s.delay,false);else s.delay=v; };
    keep.on_select = [this](Checkbox&, bool v) {if(e.control.busy()){if(v!=s.keep)keep.set_value(s.keep);}else s.keep=v; };
    amp.on_select = [this](Checkbox&, bool v) {if(e.control.busy()){if(v!=s.tx_amp)amp.set_value(s.tx_amp);}else s.tx_amp=v; };
    mode.on_change = [this](size_t, uint32_t v) {if(e.control.busy()){if(v!=s.mode)mode.set_by_value(s.mode);}else s.mode=v; };
    start.on_select = [this](Button&) {if(e.control.busy())e.halt();else{store.save();e.start();}update(true); };
    settings.on_select = [this](Button&) {if(!e.control.busy())nav_.push<RepeaterSettingsView>(s,e); };
    open.on_select = [this](Button&) {if(e.control.busy())return;auto* view=nav_.push<FileLoadView>("");view->push_dir(u"SAMPLES/REPEATER");view->on_changed=[this](std::filesystem::path p){e.select_file(p);tx.set_value(s.tx);}; };
    send.on_select = [this](Button&) {if(!e.control.busy()){store.save();e.send();} };
    rx.set_style(Theme::getInstance()->fg_cyan);
    tx.set_style(Theme::getInstance()->fg_orange);
    update(true);
}
AutoRepeaterView::~AutoRepeaterView() {
    e.halt();
    shared_memory.request_m4_performance_counter = perf;
}
void AutoRepeaterView::on_hide() {
    e.halt();
    View::on_hide();
}
void AutoRepeaterView::focus() {
    threshold.set_value(s.threshold, false);
    tx.set_value(s.tx);
    update(true);
    start.focus();
}
void AutoRepeaterView::edit(FrequencyField& field) {
    auto* target = &field;
    auto* v = nav_.push<FrequencyKeypadView>(field.value());
    v->on_changed = [target](rf::Frequency f) { target->set_value(f); };
}
void AutoRepeaterView::lock(bool busy) {
    Widget* fields[]{&rx, &tx, &threshold, &delay, &keep, &amp, &mode, &settings, &open, &send};
    for (auto* w : fields) w->set_focusable(!busy);
    rx.set_allow_digit_mode(!busy);
    tx.set_allow_digit_mode(!busy);
}
void AutoRepeaterView::update(bool force) {
    if (!refresh.ready(e.now_ms(), force)) return;
    if (!e.control.busy() && tx.value() != s.tx) tx.set_value(s.tx);
    lock(e.control.busy());
    ::auto_repeater::update_caption(start, e.control.busy() ? (e.control.automatic ? "STOP AUTO" : "STOP TX") : "START AUTO");
    threshold.set_value(s.threshold, false);
    ::auto_repeater::update_text(level, to_string_dec_int(e.level) + " dB");
    meter.levels(e.level, s.threshold);
    ::auto_repeater::update_text(state, e.state_text());
    state.set_style(e.control.state == ::auto_repeater::State::Transmitting ? Theme::getInstance()->fg_red : Theme::getInstance()->fg_cyan);
    if (s.hop && e.control.state == ::auto_repeater::State::Listening)
        ::auto_repeater::update_text(detail, "RX now " + to_string_short_freq(e.current_rx) + " MHz");
    else
        ::auto_repeater::update_text(detail, e.message);
    ::auto_repeater::update_text(counts, "REC " + to_string_dec_uint(e.recorded) + "  TX " + to_string_dec_uint(e.transmitted) + " SKIP " + to_string_dec_uint(e.skipped));
    if (s.mode == 8) {
        ::auto_repeater::update_text(protocol, std::string(e.protocol) + " " + to_string_dec_uint(e.protocol_bits) + "b");
        ::auto_repeater::update_text(decoded, "0x" + to_string_hex(e.protocol_value, 16));
    } else {
        ::auto_repeater::update_text(protocol, s.follow_rx ? "TX frequency follows RX" : "TX uses separate frequency");
        ::auto_repeater::update_text(decoded, e.file_path.filename().string());
    }
}
}  // namespace ui::external_app::auto_repeater

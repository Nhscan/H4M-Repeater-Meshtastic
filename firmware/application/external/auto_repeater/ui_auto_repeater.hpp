// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ui.hpp"
#include "ui_widget.hpp"
#include "ui_navigation.hpp"
#include "ui_receiver.hpp"
#include "app_settings.hpp"
#include "radio_state.hpp"
#include "repeater_engine.hpp"
#include "repeater_ui_helpers.hpp"
namespace ui::external_app::auto_repeater {
using Settings = ::auto_repeater::Settings;
using Engine = ::auto_repeater::Engine;
class SignalMeter : public Widget {
   public:
    SignalMeter(Rect r)
        : Widget{r} {}
    void levels(int32_t db, int32_t trigger) {
        const auto next_level = clip<int32_t>(db + 100, 0, 100);
        const auto next_threshold = clip<int32_t>(trigger + 100, 0, 100);
        if (next_level == level && next_threshold == threshold) return;
        level = next_level;
        threshold = next_threshold;
        set_dirty();
    }
    void paint(Painter& p) override {
        auto r = screen_rect();
        p.fill_rectangle(r, Color::dark_grey());
        p.fill_rectangle({r.left(), r.top(), Dim(r.width() * level / 100), r.height()}, Color::cyan());
        p.fill_rectangle({Coord(r.left() + (r.width() - 2) * threshold / 100), r.top(), 2, r.height()}, Color::yellow());
    }

   private:
    uint32_t level{0}, threshold{55};
};
class HistoryView : public View {
   public:
    HistoryView(NavigationView& nav, Engine& engine);
    void focus() override { done.focus(); }
    std::string title() const override { return "Repeater history"; }

   private:
    Text h0{}, h1{}, h2{}, h3{}, h4{}, h5{}, h6{}, h7{};
    Text* rows[8]{&h0, &h1, &h2, &h3, &h4, &h5, &h6, &h7};
    Button done{{UI_POS_X(1), UI_POS_Y(17), UI_POS_WIDTH(28), UI_POS_HEIGHT(2)}, "Done"};
};
class RepeaterSettingsView : public View {
   public:
    RepeaterSettingsView(NavigationView& nav, Settings& settings, Engine& engine);
    void focus() override { page.focus(); }
    std::string title() const override { return "Repeater settings"; }

   private:
    Settings& s;
    Engine& e;
    NavigationView& nav_;
    bool updating{false};
    uint32_t* values[8]{};
    bool* flags[3]{};
    Text t0{}, t1{}, t2{}, t3{}, t4{}, t5{}, t6{}, t7{};
    Text* labels[8]{&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7};
    NumberField n0{{UI_POS_X(22), UI_POS_Y(2)}, 7, {0, 1000000}, 1, ' '};
    NumberField n1{{UI_POS_X(22), UI_POS_Y(3)}, 7, {0, 1000000}, 1, ' '};
    NumberField n2{{UI_POS_X(22), UI_POS_Y(4)}, 7, {0, 1000000}, 1, ' '};
    NumberField n3{{UI_POS_X(22), UI_POS_Y(5)}, 7, {0, 1000000}, 1, ' '};
    NumberField n4{{UI_POS_X(22), UI_POS_Y(6)}, 7, {0, 1000000}, 1, ' '};
    NumberField n5{{UI_POS_X(22), UI_POS_Y(7)}, 7, {0, 1000000}, 1, ' '};
    NumberField n6{{UI_POS_X(22), UI_POS_Y(8)}, 7, {0, 1000000}, 1, ' '};
    NumberField n7{{UI_POS_X(22), UI_POS_Y(9)}, 7, {0, 1000000}, 1, ' '};
    NumberField* numbers[8]{&n0, &n1, &n2, &n3, &n4, &n5, &n6, &n7};
    Checkbox c0{{UI_POS_X(1), UI_POS_Y(11)}, 24, "", true};
    Checkbox c1{{UI_POS_X(1), UI_POS_Y(12)}, 24, "", true};
    Checkbox c2{{UI_POS_X(1), UI_POS_Y(13)}, 24, "", true};
    Checkbox* checks[3]{&c0, &c1, &c2};
    OptionsField page{{UI_POS_X(1), UI_POS_Y(0)}, 12, {{"Radio", 0}, {"Timing", 1}, {"Files", 2}, {"RX hopping", 3}}};
    OptionsField tx_mode{{UI_POS_X(15), UI_POS_Y(8)}, 12, {{"Same as RX", 0}, {"NFM", 1}, {"FM", 2}, {"AM", 3}, {"USB", 4}, {"LSB", 5}, {"WFM", 6}, {"DSB", 7}}};
    OptionsField rate{{UI_POS_X(15), UI_POS_Y(2)}, 12, {{"25 k", 25000}, {"50 k", 50000}, {"100 k", 100000}, {"250 k", 250000}, {"500 k", 500000}}};
    FrequencyField end_frequency{{UI_POS_X(6), UI_POS_Y(2)}};
    Text note{{UI_POS_X(1), UI_POS_Y(15), UI_POS_WIDTH(28), UI_POS_HEIGHT(1)}, ""};
    Button done{{UI_POS_X(1), UI_POS_Y(17), UI_POS_WIDTH(9), UI_POS_HEIGHT(2)}, "Done"};
    Button calibrate{{UI_POS_X(10), UI_POS_Y(17), UI_POS_WIDTH(10), UI_POS_HEIGHT(2)}, "Auto level"};
    Button history{{UI_POS_X(20), UI_POS_Y(17), UI_POS_WIDTH(9), UI_POS_HEIGHT(2)}, "History"};
    void show_page(uint32_t index);
    void row(size_t i, const char* label, uint32_t& value, int32_t min, int32_t max, int32_t step = 1);
    void flag(size_t i, const char* label, bool& value);
};
class AutoRepeaterView : public View {
   public:
    AutoRepeaterView(NavigationView& nav);
    ~AutoRepeaterView();
    void focus() override;
    void on_hide() override;
    std::string title() const override { return "Auto Repeater 0.3.1"; }

   private:
    NavigationView& nav_;
    Settings s{};
    RxRadioState rx_state{ReceiverModel::Mode::Capture};
    TxRadioState tx_state{};
    SettingsStore store{"auto_repeater", {{"rx"sv, &s.rx}, {"tx"sv, &s.tx}, {"threshold"sv, &s.threshold}, {"rate"sv, &s.rate}, {"delay"sv, &s.delay}, {"keep"sv, &s.keep}, {"attack"sv, &s.attack}, {"release"sv, &s.release}, {"maximum"sv, &s.maximum}, {"cooldown"sv, &s.cooldown}, {"hysteresis"sv, &s.hysteresis}, {"lna"sv, &s.lna}, {"vga"sv, &s.vga}, {"tx_gain"sv, &s.tx_gain}, {"mode"sv, &s.mode}, {"tx_mode"sv, &s.tx_mode}, {"repeats"sv, &s.repeats}, {"gap"sv, &s.gap}, {"audio_gain"sv, &s.audio_gain}, {"tx_amp"sv, &s.tx_amp}, {"rx_amp"sv, &s.rx_amp}, {"monitor"sv, &s.monitor}, {"wait_quiet"sv, &s.wait_quiet}, {"record_only"sv, &s.record_only}, {"log"sv, &s.log}, {"sub_save"sv, &s.sub_save}, {"sub_threshold"sv, &s.sub_threshold}, {"prebuffer_ms"sv, &s.prebuffer_ms}, {"hop"sv, &s.hop}, {"hop_end"sv, &s.hop_end}, {"hop_step"sv, &s.hop_step}, {"dwell"sv, &s.dwell}, {"follow_rx"sv, &s.follow_rx}}};
    Engine e{s};
    uint32_t perf{0};
    ::auto_repeater::RefreshGate refresh{};
    Labels labels{
        {{UI_POS_X(1), UI_POS_Y(0)}, "RECEIVE / HOP START   MHz", Color::cyan()},
        {{UI_POS_X(1), UI_POS_Y(2)}, "TRANSMIT             MHz", Color::orange()},
        {{UI_POS_X(1), UI_POS_Y(4)}, "Trigger:", Color::light_grey()},
        {{UI_POS_X(1), UI_POS_Y(6)}, "Mode:", Color::light_grey()},
        {{UI_POS_X(1), UI_POS_Y(7)}, "TX delay:    s", Color::light_grey()}};
    FrequencyField rx{{UI_POS_X(1), UI_POS_Y(1)}}, tx{{UI_POS_X(1), UI_POS_Y(3)}};
    NumberField threshold{{UI_POS_X(10), UI_POS_Y(4)}, 4, {-100, 0}, 1, ' '};
    Text level{{UI_POS_X(17), UI_POS_Y(4), UI_POS_WIDTH(12), UI_POS_HEIGHT(1)}, "-- dB"};
    SignalMeter meter{{UI_POS_X(1), UI_POS_Y(5) + 4, UI_POS_WIDTH(28), 8}};
    OptionsField mode{{UI_POS_X(7), UI_POS_Y(6)}, 12, {{"Raw IQ", 0}, {"NFM voice", 1}, {"FM voice", 2}, {"AM voice", 3}, {"USB voice", 4}, {"LSB voice", 5}, {"WFM voice", 6}, {"OOK decode", 8}}};
    NumberField delay{{UI_POS_X(11), UI_POS_Y(7)}, 2, {1, 30}, 1, ' ', false};
    Checkbox keep{{UI_POS_X(1), UI_POS_Y(8)}, 12, "Save", true}, amp{{UI_POS_X(16), UI_POS_Y(8)}, 10, "TX AMP", true};
    Text protocol{{UI_POS_X(1), UI_POS_Y(9), UI_POS_WIDTH(28), UI_POS_HEIGHT(1)}, "Protocol: Unknown"};
    Text state{{UI_POS_X(1), UI_POS_Y(10), UI_POS_WIDTH(28), UI_POS_HEIGHT(1)}, "STOPPED"};
    Text detail{{UI_POS_X(1), UI_POS_Y(11), UI_POS_WIDTH(28), UI_POS_HEIGHT(1)}, ""};
    Text counts{{UI_POS_X(1), UI_POS_Y(12), UI_POS_WIDTH(28), UI_POS_HEIGHT(1)}, ""};
    Text decoded{{UI_POS_X(1), UI_POS_Y(13), UI_POS_WIDTH(28), UI_POS_HEIGHT(1)}, ""};
    Button start{{UI_POS_X(1), UI_POS_Y(14), UI_POS_WIDTH(28), UI_POS_HEIGHT(2)}, "START AUTO"};
    Button settings{{UI_POS_X(1), UI_POS_Y(17), UI_POS_WIDTH(10), UI_POS_HEIGHT(2)}, "Settings"};
    Button open{{UI_POS_X(11), UI_POS_Y(17), UI_POS_WIDTH(9), UI_POS_HEIGHT(2)}, "Open"};
    Button send{{UI_POS_X(20), UI_POS_Y(17), UI_POS_WIDTH(9), UI_POS_HEIGHT(2)}, "Send"};
    void update(bool force = false);
    void edit(FrequencyField& field);
    void lock(bool busy);
    MessageHandlerRegistration stats_handler{Message::ID::ChannelStatistics, [this](const Message* p) { e.statistics(static_cast<const ChannelStatisticsMessage*>(p)->statistics); }};
    MessageHandlerRegistration frame_handler{Message::ID::DisplayFrameSync, [this](const Message*) {e.tick();update(); }};
    MessageHandlerRegistration capture_handler{Message::ID::CaptureThreadDone, [this](const Message* p) { e.capture_done(static_cast<const CaptureThreadDoneMessage*>(p)->error); }};
    MessageHandlerRegistration replay_handler{Message::ID::ReplayThreadDone, [this](const Message* p) { e.tx_done(static_cast<const ReplayThreadDoneMessage*>(p)->return_code); }};
    MessageHandlerRegistration ready_handler{Message::ID::RequestSignal, [this](const Message* p) {if(static_cast<const RequestSignalMessage*>(p)->signal==RequestSignalMessage::Signal::FillRequest)e.ready=true; }};
    MessageHandlerRegistration protocol_handler{Message::ID::SubGhzDData, [this](const Message* p) { e.decoded(*static_cast<const SubGhzDDataMessage*>(p)); }};
};
}  // namespace ui::external_app::auto_repeater

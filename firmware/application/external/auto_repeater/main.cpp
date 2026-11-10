// SPDX-License-Identifier: GPL-2.0-or-later
#include "ui_auto_repeater.hpp"
#include "external_app.hpp"
namespace ui::external_app::auto_repeater {
void initialize_app(NavigationView& nav) {
    nav.push<AutoRepeaterView>();
}
}  // namespace ui::external_app::auto_repeater
extern "C" {
__attribute__((section(".external_app.app_auto_repeater.application_information"), used))
application_information_t _application_information_auto_repeater = {
    (uint8_t*)0,
    ui::external_app::auto_repeater::initialize_app,
    CURRENT_HEADER_VERSION,
    VERSION_MD5,
    "Auto Repeater",
    {0, 0, 0xf0, 0x0f, 0x18, 0x18, 0xc, 0x30, 6, 0x60, 3, 0xc0, 0x1f, 0xf8, 0xf, 0xf0,
     0xf, 0xf0, 0x1f, 0xf8, 3, 0xc0, 6, 0x60, 0xc, 0x30, 0x18, 0x18, 0xf0, 0xf, 0, 0},
    ui::Color::cyan().v,
    app_location_t::TRX,
    -1,
    // Reserve Replay image space below the app. Capture is smaller in this pinned build.
    // Both SPI images may then be loaded without overwriting the M0 app.
    {'A', 'R', 'S', 'V'},
    0};
}

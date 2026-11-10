// SPDX-License-Identifier: GPL-2.0-or-later
#include "ui.hpp"
#include "ui_navigation.hpp"
#include "external_app.hpp"
namespace ui::external_app::meshtastic {
void initialize_app(NavigationView&);
}
extern "C" {
__attribute__((section(".external_app.app_meshtastic.application_information"), used))
application_information_t _application_information_meshtastic = {
    (uint8_t*)0,
    ui::external_app::meshtastic::initialize_app,
    CURRENT_HEADER_VERSION,
    VERSION_MD5,
    "Meshtastic",
    {0, 0, 0x80, 1, 0xc0, 3, 0xe0, 7, 0xf0, 15, 0x98, 25, 0x8c, 49, 0x86, 97, 0x86, 97, 0x8c, 49, 0x98, 25, 0xf0, 15, 0xe0, 7, 0xc0, 3, 0x80, 1, 0, 0},
    ui::Color::green().v,
    app_location_t::TRX,
    -1,
    {'M', 'S', 'R', 'V'},
    0};
}

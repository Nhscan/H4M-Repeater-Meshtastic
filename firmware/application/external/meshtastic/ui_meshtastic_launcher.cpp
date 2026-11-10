// SPDX-License-Identifier: GPL-2.0-or-later
#include "ui_meshtastic.hpp"
#include "ui_navigation.hpp"
namespace ui::external_app::meshtastic {
void initialize_app(NavigationView& nav) {
    nav.push<ui::MeshtasticView>();
}
}  // namespace ui::external_app::meshtastic

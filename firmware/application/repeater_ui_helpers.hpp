// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace auto_repeater {
class RefreshGate {
   public:
    bool ready(uint32_t now, bool force = false) {
        if (initialized && !force && now - last < 200) return false;
        initialized = true;
        last = now;
        return true;
    }

   private:
    bool initialized{false};
    uint32_t last{0};
};
template <typename Text>
void update_text(Text& widget, std::string_view value) {
    if (widget.get() != value) widget.set(value);
}
template <typename Button>
void update_caption(Button& widget, const char* value) {
    if (widget.text() != value) widget.set_text(value);
}
}  // namespace auto_repeater

#pragma once

#include <imgui.h>

#include <string>
#include <vector>

namespace sims {
namespace ui {

// A simple radial pie menu drawn via ImGui's foreground ImDrawList. The
// caller opens it at a screen-space anchor and provides a list of labeled
// slices; the menu handles hover, click, and cancellation (right-click or
// Esc). On selection the callback is invoked with the slice index; -1 is
// reported on cancel.
//
// Usage:
//   if (pie.open) pie.draw("Fridge", slice_labels, [&](int i){ ... });
struct PieMenu {
    bool open = false;
    ImVec2 anchor{};       // screen-space center
    float radius = 70.0f;
    int hovered = -1;
    bool first_frame = false;

    void show_at(ImVec2 screen_pos) {
        anchor = screen_pos;
        open = true;
        hovered = -1;
        first_frame = true;
    }
    void close() { open = false; hovered = -1; first_frame = false; }

    // Draws the menu if `open`. Returns true once a slice is selected or the
    // menu is cancelled; check `hovered` (-1 = cancelled) for the result.
    // `labels` must stay alive for the duration of the call.
    bool draw(const char* title, const std::vector<std::string>& labels);
};

} // namespace ui
} // namespace sims

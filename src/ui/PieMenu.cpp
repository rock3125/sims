#include "ui/PieMenu.hpp"

#include <imgui.h>

#include <cmath>

namespace sims {
namespace ui {

namespace {

constexpr float kPi = 3.14159265358979323846f;

} // namespace

bool PieMenu::draw(const char* title, const std::vector<std::string>& labels) {
    if (!open) return false;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    const int n = static_cast<int>(labels.size());
    if (n == 0) { close(); return true; }

    // Each slice spans 2pi/n. Slice 0 is centered on -90deg (up).
    float slice = 2.0f * kPi / static_cast<float>(n);

    ImVec2 mouse = io.MousePos;
    float mdx = mouse.x - anchor.x;
    float mdy = mouse.y - anchor.y;
    float mdist = std::sqrt(mdx * mdx + mdy * mdy);

    int new_hover = -1;
    if (mdist <= radius && mdist > 6.0f) {
        float ang = std::atan2(mdy, mdx); // [-pi, pi], 0 = +x (right)
        // Rotate so slice 0's center (up, -pi/2) maps to angle 0.
        float a = ang + kPi * 0.5f;
        if (a < 0.0f) a += 2.0f * kPi;
        if (a >= 2.0f * kPi) a -= 2.0f * kPi;
        // Shift by half a slice so the boundary aligns with slice edges.
        float shifted = a + slice * 0.5f;
        if (shifted >= 2.0f * kPi) shifted -= 2.0f * kPi;
        new_hover = static_cast<int>(shifted / slice);
        if (new_hover < 0 || new_hover >= n) new_hover = -1;
    } else if (mdist <= 6.0f) {
        new_hover = -1; // dead zone at center
    }
    hovered = new_hover;

    // Draw slices.
    for (int i = 0; i < n; ++i) {
        float center = -kPi * 0.5f + slice * static_cast<float>(i);
        float a0 = center - slice * 0.5f;
        float a1 = center + slice * 0.5f;
        ImU32 fill = (i == hovered)
                     ? IM_COL32(80, 140, 220, 220)
                     : IM_COL32(40, 50, 70, 200);
        // Use PathArcTo for the wedge.
        dl->PathArcTo(anchor, radius, a0, a1, 24);
        dl->PathLineTo(anchor);
        dl->PathFillConvex(fill);
    }

    // Outer ring + inner hub.
    dl->AddCircle(anchor, radius, IM_COL32(180, 190, 210, 255), 48, 1.5f);
    dl->AddCircleFilled(anchor, 6.0f, IM_COL32(60, 70, 90, 255), 24);

    // Labels.
    for (int i = 0; i < n; ++i) {
        float center = -kPi * 0.5f + slice * static_cast<float>(i);
        float lr = radius * 0.65f;
        ImVec2 lp(anchor.x + std::cos(center) * lr,
                  anchor.y + std::sin(center) * lr);
        ImU32 text_col = (i == hovered) ? IM_COL32(255, 255, 255, 255)
                                        : IM_COL32(220, 220, 220, 230);
        const char* txt = labels[i].c_str();
        ImVec2 ts = ImGui::CalcTextSize(txt);
        dl->AddRectFilled(ImVec2(lp.x - ts.x * 0.5f - 3, lp.y - ts.y * 0.5f - 2),
                          ImVec2(lp.x + ts.x * 0.5f + 3, lp.y + ts.y * 0.5f + 2),
                          IM_COL32(0, 0, 0, 160), 4.0f);
        dl->AddText(ImVec2(lp.x - ts.x * 0.5f, lp.y - ts.y * 0.5f), text_col, txt);
    }

    // Title above the pie.
    {
        const char* t = title ? title : "";
        ImVec2 ts = ImGui::CalcTextSize(t);
        dl->AddText(ImVec2(anchor.x - ts.x * 0.5f, anchor.y - radius - 22.0f),
                    IM_COL32(255, 255, 255, 230), t);
    }

    // Consume the click that opened the menu so it doesn't immediately
    // select slice 0.
    bool clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !first_frame;
    bool rclicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    bool escaped = ImGui::IsKeyPressed(ImGuiKey_Escape, false);

    first_frame = false;

    if (escaped || rclicked) {
        close();
        return true;
    }
    if (clicked) {
        int sel = hovered;
        close();
        hovered = sel;
        return true;
    }
    return false;
}

} // namespace ui
} // namespace sims

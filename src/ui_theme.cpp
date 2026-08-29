#include "ui_theme.h"

#include <algorithm>

#include "imgui_internal.h" // ImGuiWindow, SkipItems

namespace theme {

ImVec4 BackgroundColor()   { return ImVec4(0.039f, 0.051f, 0.071f, 1.00f); } // #0A0D12
ImVec4 PanelColor()        { return ImVec4(0.063f, 0.078f, 0.110f, 1.00f); } // #10141C
ImVec4 PanelAltColor()     { return ImVec4(0.082f, 0.106f, 0.149f, 1.00f); } // #151B26
ImVec4 BorderColor()       { return ImVec4(0.165f, 0.200f, 0.259f, 0.55f); } // #2A3342
ImVec4 AccentPrimary()     { return ImVec4(0.184f, 0.851f, 0.769f, 1.00f); } // #2FD9C4 signal teal
ImVec4 AccentSecondary()   { return ImVec4(0.310f, 0.659f, 1.000f, 1.00f); } // #4FA8FF steel blue
ImVec4 TextPrimary()       { return ImVec4(0.906f, 0.906f, 0.937f, 1.00f); } // #E6E6EF
ImVec4 TextMuted()         { return ImVec4(0.541f, 0.541f, 0.600f, 1.00f); } // #8A8A99
ImVec4 Danger()            { return ImVec4(1.000f, 0.361f, 0.478f, 1.00f); } // #FF5C7A
ImVec4 Warning()           { return ImVec4(1.000f, 0.706f, 0.329f, 1.00f); } // #FFB454
ImVec4 Success()           { return ImVec4(0.318f, 0.878f, 0.596f, 1.00f); } // #51E098

void ApplyCoreViewStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // --- Geometry: rounded, airy, low-noise borders ---
    style.WindowRounding    = 10.0f;
    style.ChildRounding     = 10.0f;
    style.FrameRounding     = 7.0f;
    style.PopupRounding     = 9.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding      = 12.0f;
    style.TabRounding       = 8.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;

    style.WindowPadding     = ImVec2(14, 12);
    style.FramePadding      = ImVec2(10, 6);
    style.CellPadding       = ImVec2(10, 6);
    style.ItemSpacing       = ImVec2(10, 8);
    style.ItemInnerSpacing  = ImVec2(8, 6);
    style.IndentSpacing     = 18.0f;
    style.ScrollbarSize     = 12.0f;
    style.GrabMinSize       = 10.0f;

    style.WindowTitleAlign  = ImVec2(0.0f, 0.5f);
    style.SeparatorTextBorderSize = 1.0f;

    ImVec4 bg      = BackgroundColor();
    ImVec4 panel   = PanelColor();
    ImVec4 panelAlt= PanelAltColor();
    ImVec4 border  = BorderColor();
    ImVec4 accent  = AccentPrimary();
    ImVec4 accent2 = AccentSecondary();
    ImVec4 text    = TextPrimary();
    ImVec4 muted   = TextMuted();

    auto with_alpha = [](ImVec4 c, float a) { c.w = a; return c; };

    colors[ImGuiCol_Text]                  = text;
    colors[ImGuiCol_TextDisabled]          = muted;
    colors[ImGuiCol_WindowBg]              = bg;
    colors[ImGuiCol_ChildBg]               = panel;
    colors[ImGuiCol_PopupBg]               = with_alpha(panelAlt, 0.98f);
    colors[ImGuiCol_Border]                = border;
    colors[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);

    colors[ImGuiCol_FrameBg]               = panelAlt;
    colors[ImGuiCol_FrameBgHovered]        = with_alpha(accent, 0.25f);
    colors[ImGuiCol_FrameBgActive]         = with_alpha(accent, 0.40f);

    colors[ImGuiCol_TitleBg]               = bg;
    colors[ImGuiCol_TitleBgActive]         = bg;
    colors[ImGuiCol_TitleBgCollapsed]      = bg;
    colors[ImGuiCol_MenuBarBg]             = panel;

    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ScrollbarGrab]         = with_alpha(accent, 0.35f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = with_alpha(accent, 0.55f);
    colors[ImGuiCol_ScrollbarGrabActive]   = with_alpha(accent, 0.75f);

    colors[ImGuiCol_CheckMark]             = accent2;
    colors[ImGuiCol_SliderGrab]            = accent;
    colors[ImGuiCol_SliderGrabActive]      = accent2;

    colors[ImGuiCol_Button]                = with_alpha(accent, 0.22f);
    colors[ImGuiCol_ButtonHovered]         = with_alpha(accent, 0.45f);
    colors[ImGuiCol_ButtonActive]          = with_alpha(accent, 0.65f);

    colors[ImGuiCol_Header]                = with_alpha(accent, 0.28f);
    colors[ImGuiCol_HeaderHovered]         = with_alpha(accent, 0.45f);
    colors[ImGuiCol_HeaderActive]          = with_alpha(accent, 0.60f);

    colors[ImGuiCol_Separator]             = border;
    colors[ImGuiCol_SeparatorHovered]      = accent2;
    colors[ImGuiCol_SeparatorActive]       = accent2;

    colors[ImGuiCol_ResizeGrip]            = with_alpha(accent, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]     = with_alpha(accent, 0.50f);
    colors[ImGuiCol_ResizeGripActive]      = with_alpha(accent, 0.75f);

    colors[ImGuiCol_Tab]                   = panel;
    colors[ImGuiCol_TabHovered]            = with_alpha(accent, 0.45f);
    colors[ImGuiCol_TabActive]             = with_alpha(accent, 0.35f);
    colors[ImGuiCol_TabUnfocused]          = panel;
    colors[ImGuiCol_TabUnfocusedActive]    = with_alpha(accent, 0.20f);

    colors[ImGuiCol_TableHeaderBg]         = panelAlt;
    colors[ImGuiCol_TableBorderStrong]     = border;
    colors[ImGuiCol_TableBorderLight]      = with_alpha(border, 0.35f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_TableRowBgAlt]         = with_alpha(ImVec4(1, 1, 1, 1), 0.02f);

    colors[ImGuiCol_TextSelectedBg]        = with_alpha(accent, 0.35f);
    colors[ImGuiCol_DragDropTarget]        = accent2;
    colors[ImGuiCol_NavHighlight]          = accent2;
    colors[ImGuiCol_PlotLines]             = accent2;
    colors[ImGuiCol_PlotLinesHovered]      = accent;
    colors[ImGuiCol_PlotHistogram]         = accent;
    colors[ImGuiCol_PlotHistogramHovered]  = accent2;
}

void DrawGlowRect(ImDrawList* draw_list, ImVec2 min, ImVec2 max, ImU32 color,
                   float rounding, int layers, float spread) {
    if (layers <= 0) return;
    float base_alpha = (float)((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
    for (int i = layers; i >= 1; --i) {
        float t = (float)i / (float)layers;
        float grow = spread * t;
        float alpha = base_alpha * (1.0f - t) * (1.0f - t) * 0.55f;
        if (alpha <= 0.003f) continue;
        ImU32 c = IM_COL32(
            (color >> IM_COL32_R_SHIFT) & 0xFF,
            (color >> IM_COL32_G_SHIFT) & 0xFF,
            (color >> IM_COL32_B_SHIFT) & 0xFF,
            (int)(alpha * 255.0f));
        draw_list->AddRectFilled(
            ImVec2(min.x - grow, min.y - grow),
            ImVec2(max.x + grow, max.y + grow),
            c, rounding + grow * 0.5f);
    }
}

void GlowCard(ImVec2 min, ImVec2 max, float rounding, ImVec4 color) {
    if (color.w == 0.0f && color.x == 0.0f && color.y == 0.0f && color.z == 0.0f)
        color = AccentPrimary();
    ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 0.35f));
    DrawGlowRect(ImGui::GetWindowDrawList(), min, max, col, rounding);
}

void MeterBar(const char* label, float value, ImVec2 size, ImVec4 color, const char* overlay_text) {
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float rounding = size.y * 0.5f;

    ImVec4 track = PanelAltColor();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                       ImGui::ColorConvertFloat4ToU32(track), rounding);

    float fill_w = size.x * value;
    if (fill_w > 1.0f) {
        ImU32 fill_col = ImGui::ColorConvertFloat4ToU32(color);
        // subtle glow under high load
        if (value > 0.75f) {
            ImU32 glow = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 0.4f));
            DrawGlowRect(dl, pos, ImVec2(pos.x + fill_w, pos.y + size.y), glow, rounding, 3, 6.0f);
        }
        dl->AddRectFilled(pos, ImVec2(pos.x + fill_w, pos.y + size.y), fill_col, rounding);
    }

    if (overlay_text) {
        ImVec2 text_size = ImGui::CalcTextSize(overlay_text);
        ImVec2 text_pos(pos.x + (size.x - text_size.x) * 0.5f, pos.y + (size.y - text_size.y) * 0.5f);
        dl->AddText(text_pos, ImGui::ColorConvertFloat4ToU32(TextPrimary()), overlay_text);
    }

    ImGui::Dummy(size);
    ImGui::SetItemTooltip("%s: %.0f%%", label, value * 100.0f);
}

} // namespace theme

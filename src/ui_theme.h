#pragma once

#include "imgui.h"

// CoreView visual identity: graphite-slate surfaces, teal/steel-blue accents,
// soft rounded geometry and a hand-rolled glow effect (ImGui has no native
// blur/shadow, so glow is faked with nested translucent rounded rects).
namespace theme {

// Core palette. Kept as functions (not globals) so callers always get
// live ImVec4 values without worrying about static init order.
ImVec4 BackgroundColor();      // app canvas, deepest layer
ImVec4 PanelColor();           // cards / child windows
ImVec4 PanelAltColor();        // alternating row / nested panel
ImVec4 BorderColor();
ImVec4 AccentPrimary();        // signal teal
ImVec4 AccentSecondary();      // steel blue
ImVec4 TextPrimary();
ImVec4 TextMuted();
ImVec4 Danger();
ImVec4 Warning();
ImVec4 Success();

// Applies the full CoreView ImGuiStyle (colors, rounding, spacing, glow-friendly
// borders). Call once after ImGui::CreateContext().
void ApplyCoreViewStyle();

// Draws a soft glow behind a rectangle: several nested translucent rounded
// borders with falling alpha, growing outward from [min,max].
void DrawGlowRect(ImDrawList* draw_list, ImVec2 min, ImVec2 max, ImU32 color,
                   float rounding = 8.0f, int layers = 5, float spread = 10.0f);

// Convenience: glow using AccentPrimary at a fixed low intensity, meant to be
// called immediately before BeginChild()/rendering a card so the glow sits
// underneath it.
void GlowCard(ImVec2 min, ImVec2 max, float rounding = 10.0f, ImVec4 color = ImVec4(0, 0, 0, 0));

// A slim colored horizontal bar with rounded ends, used for per-core CPU bars
// and other compact meters. value in [0,1].
void MeterBar(const char* label, float value, ImVec2 size, ImVec4 color, const char* overlay_text = nullptr);

} // namespace theme

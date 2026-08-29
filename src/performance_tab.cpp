#include "performance_tab.h"

#include <algorithm>
#include <cstdio>

#include "imgui.h"
#include "ui_theme.h"

namespace {

// Draws a titled "card": a bordered, glow-backed child region. Caller draws
// content between BeginCard/EndCard.
bool BeginCard(const char* id, const char* title, float height) {
    ImVec2 min = ImGui::GetCursorScreenPos();
    ImVec2 size(ImGui::GetContentRegionAvail().x, height);
    theme::GlowCard(min, ImVec2(min.x + size.x, min.y + size.y), 10.0f,
                     ImVec4(theme::AccentSecondary().x, theme::AccentSecondary().y, theme::AccentSecondary().z, 0.2f));

    bool open = ImGui::BeginChild(id, size, ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::AccentSecondary());
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImGui::Separator();
    return open;
}

void EndCard() {
    ImGui::EndChild();
}

void PlotHistory(const char* label, const RingBuffer& history, float height, ImVec4 color,
                  float max_hint = 100.0f) {
    std::vector<float> values = history.Linearized();
    if (values.empty()) values.push_back(0.0f);
    ImGui::PushStyleColor(ImGuiCol_PlotLines, color);
    ImGui::PlotLines(label, values.data(), (int)values.size(), 0, nullptr,
                      0.0f, max_hint, ImVec2(-1, height));
    ImGui::PopStyleColor();
}

} // namespace

PerformanceTab::PerformanceTab() {}

void PerformanceTab::Refresh() {
    cpu_ = sysinfo::GetCpuSample();
    mem_ = sysinfo::GetMemSample();
    disk_ = sysinfo::GetDiskSample();
    net_ = sysinfo::GetNetSample();
    uptime_seconds_ = sysinfo::GetUptimeSeconds();

    cpu_history_.Push((float)cpu_.overall_percent);
    double mem_pct = mem_.total_bytes > 0 ? (100.0 * (double)mem_.used_bytes / (double)mem_.total_bytes) : 0.0;
    mem_history_.Push((float)mem_pct);
    disk_read_history_.Push((float)disk_.read_bytes_per_sec);
    disk_write_history_.Push((float)disk_.write_bytes_per_sec);
    net_recv_history_.Push((float)net_.recv_bytes_per_sec);
    net_send_history_.Push((float)net_.send_bytes_per_sec);
}

void PerformanceTab::Draw() {
    ImGui::BeginChild("##perf_scroll", ImVec2(0, 0), ImGuiChildFlags_None);

    DrawCpuCard();
    ImGui::Dummy(ImVec2(0, 6));
    DrawMemoryCard();
    ImGui::Dummy(ImVec2(0, 6));
    DrawDiskCard();
    ImGui::Dummy(ImVec2(0, 6));
    DrawNetworkCard();
    ImGui::Dummy(ImVec2(0, 6));
    DrawUptimeCard();

    ImGui::EndChild();
}

void PerformanceTab::DrawCpuCard() {
    int n_cores = (int)cpu_.per_core_percent.size();
    float core_rows_height = n_cores > 0 ? (float)((n_cores + 1) / 2) * 24.0f : 0.0f;
    float height = 90.0f + core_rows_height + 30.0f;

    if (BeginCard("##cpu_card", "CPU", height)) {
        ImGui::TextColored(theme::TextPrimary(), "Overall: %.1f%%", cpu_.overall_percent);
        PlotHistory("##cpu_plot", cpu_history_, 60.0f, theme::AccentPrimary());

        if (n_cores > 0) {
            ImGui::Spacing();
            ImGui::TextColored(theme::TextMuted(), "Per-core (%d)", n_cores);
            float avail = ImGui::GetContentRegionAvail().x;
            float bar_w = (avail - 10.0f) / 2.0f;
            for (int i = 0; i < n_cores; ++i) {
                if (i % 2 != 0) ImGui::SameLine(0, 10.0f);
                float v = (float)cpu_.per_core_percent[i] / 100.0f;
                ImVec4 col = v > 0.85f ? theme::Danger() : v > 0.5f ? theme::Warning() : theme::AccentSecondary();
                char lbl[32];
                std::snprintf(lbl, sizeof(lbl), "Core %d", i);
                char overlay[16];
                std::snprintf(overlay, sizeof(overlay), "%.0f%%", cpu_.per_core_percent[i]);
                ImGui::TextColored(theme::TextMuted(), "%2d", i);
                ImGui::SameLine(0, 6);
                theme::MeterBar(lbl, v, ImVec2(bar_w - 30.0f, 16.0f), col, overlay);
            }
        }
    }
    EndCard();
}

void PerformanceTab::DrawMemoryCard() {
    if (BeginCard("##mem_card", "MEMORY", 150.0f)) {
        double pct = mem_.total_bytes > 0 ? (100.0 * (double)mem_.used_bytes / (double)mem_.total_bytes) : 0.0;
        ImGui::TextColored(theme::TextPrimary(), "%s / %s  (%.0f%%)",
                            sysinfo::FormatBytes(mem_.used_bytes).c_str(),
                            sysinfo::FormatBytes(mem_.total_bytes).c_str(), pct);
        ImVec4 col = pct > 85.0 ? theme::Danger() : pct > 60.0 ? theme::Warning() : theme::Success();
        theme::MeterBar("Memory", (float)(pct / 100.0), ImVec2(ImGui::GetContentRegionAvail().x, 18.0f), col);
        PlotHistory("##mem_plot", mem_history_, 60.0f, theme::AccentSecondary());
    }
    EndCard();
}

void PerformanceTab::DrawDiskCard() {
    if (BeginCard("##disk_card", "DISK", 150.0f)) {
        ImGui::TextColored(theme::Success(), "Read: %s", sysinfo::FormatRate(disk_.read_bytes_per_sec).c_str());
        ImGui::SameLine(0, 24);
        ImGui::TextColored(theme::Warning(), "Write: %s", sysinfo::FormatRate(disk_.write_bytes_per_sec).c_str());

        float max_hint = 1.0f;
        for (float v : disk_read_history_.Linearized()) max_hint = std::max(max_hint, v);
        for (float v : disk_write_history_.Linearized()) max_hint = std::max(max_hint, v);
        max_hint *= 1.2f;

        PlotHistory("##disk_read_plot", disk_read_history_, 50.0f, theme::Success(), max_hint);
        PlotHistory("##disk_write_plot", disk_write_history_, 50.0f, theme::Warning(), max_hint);
    }
    EndCard();
}

void PerformanceTab::DrawNetworkCard() {
    if (BeginCard("##net_card", "NETWORK", 150.0f)) {
        ImGui::TextColored(theme::AccentSecondary(), "Recv: %s", sysinfo::FormatRate(net_.recv_bytes_per_sec).c_str());
        ImGui::SameLine(0, 24);
        ImGui::TextColored(theme::AccentPrimary(), "Send: %s", sysinfo::FormatRate(net_.send_bytes_per_sec).c_str());

        float max_hint = 1.0f;
        for (float v : net_recv_history_.Linearized()) max_hint = std::max(max_hint, v);
        for (float v : net_send_history_.Linearized()) max_hint = std::max(max_hint, v);
        max_hint *= 1.2f;

        PlotHistory("##net_recv_plot", net_recv_history_, 50.0f, theme::AccentSecondary(), max_hint);
        PlotHistory("##net_send_plot", net_send_history_, 50.0f, theme::AccentPrimary(), max_hint);
    }
    EndCard();
}

void PerformanceTab::DrawUptimeCard() {
    if (BeginCard("##uptime_card", "UPTIME", 60.0f)) {
        ImGui::TextColored(theme::TextPrimary(), "%s", sysinfo::FormatUptime(uptime_seconds_).c_str());
    }
    EndCard();
}

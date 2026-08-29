#include "logs_tab.h"

#include <cstdio>
#include <ctime>

#include "imgui.h"
#include "ui_theme.h"

namespace {

std::string NowTimestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);
    return buf;
}

ImVec4 LevelColor(LogsTab::Level level) {
    switch (level) {
        case LogsTab::Level::Success: return theme::Success();
        case LogsTab::Level::Warn:    return theme::Warning();
        case LogsTab::Level::Error:   return theme::Danger();
        default:                      return theme::TextMuted();
    }
}

const char* LevelTag(LogsTab::Level level) {
    switch (level) {
        case LogsTab::Level::Success: return "START";
        case LogsTab::Level::Warn:    return "WARN ";
        case LogsTab::Level::Error:   return "STOP ";
        default:                      return "INFO ";
    }
}

} // namespace

LogsTab::LogsTab() {
    AddEvent(Level::Info, "CoreView Task Manager initialized.");
}

void LogsTab::AddEvent(Level level, const std::string& message) {
    entries_.push_back({NowTimestamp(), message, level});
    while (entries_.size() > kMaxEntries)
        entries_.pop_front();
}

void LogsTab::Draw() {
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::PanelAltColor());
    if (focus_filter_next_frame_) {
        ImGui::SetKeyboardFocusHere();
        focus_filter_next_frame_ = false;
    }
    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##logfilter", "Filter messages...", filter_buf_, sizeof(filter_buf_));
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::TextColored(theme::Success(), "START"); ImGui::SameLine();
    ImGui::Checkbox("##show_success", &show_success_); ImGui::SameLine(0, 14);

    ImGui::TextColored(theme::Danger(), "STOP"); ImGui::SameLine();
    ImGui::Checkbox("##show_error", &show_error_); ImGui::SameLine(0, 14);

    ImGui::TextColored(theme::Warning(), "WARN"); ImGui::SameLine();
    ImGui::Checkbox("##show_warn", &show_warn_); ImGui::SameLine(0, 14);

    ImGui::TextColored(theme::TextMuted(), "INFO"); ImGui::SameLine();
    ImGui::Checkbox("##show_info", &show_info_);

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 190);
    ImGui::Checkbox("Auto-scroll", &auto_scroll_);
    ImGui::SameLine();
    if (ImGui::Button("Clear")) entries_.clear();

    ImGui::Separator();

    ImGui::BeginChild("##log_scroll", ImVec2(0, 0), ImGuiChildFlags_Border,
                       ImGuiWindowFlags_HorizontalScrollbar);

    std::string filter_lower(filter_buf_);
    for (auto& c : filter_lower) c = (char)tolower((unsigned char)c);

    for (const Entry& e : entries_) {
        if (e.level == Level::Info && !show_info_) continue;
        if (e.level == Level::Success && !show_success_) continue;
        if (e.level == Level::Warn && !show_warn_) continue;
        if (e.level == Level::Error && !show_error_) continue;

        if (!filter_lower.empty()) {
            std::string msg_lower = e.message;
            for (auto& c : msg_lower) c = (char)tolower((unsigned char)c);
            if (msg_lower.find(filter_lower) == std::string::npos) continue;
        }

        ImGui::TextColored(theme::TextMuted(), "%s", e.timestamp.c_str());
        ImGui::SameLine();
        ImGui::TextColored(LevelColor(e.level), "%s", LevelTag(e.level));
        ImGui::SameLine();
        ImGui::TextColored(theme::TextPrimary(), "%s", e.message.c_str());
    }

    if (auto_scroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
}

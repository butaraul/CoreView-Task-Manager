#include "process_tab.h"

#include <algorithm>
#include <cstdio>

#include "logs_tab.h"
#include "ui_theme.h"

namespace {

enum ColumnId {
    Col_Name = 0,
    Col_Pid,
    Col_Cpu,
    Col_Mem,
    Col_Status,
    Col_User,
};

std::string ToLower(const std::string& s) {
    std::string out = s;
    for (auto& c : out) c = (char)tolower((unsigned char)c);
    return out;
}

ImVec4 StatusColor(sysinfo::ProcessStatus status) {
    switch (status) {
        case sysinfo::ProcessStatus::Running:  return theme::Success();
        case sysinfo::ProcessStatus::Sleeping: return theme::TextMuted();
        case sysinfo::ProcessStatus::Stopped:  return theme::Warning();
        case sysinfo::ProcessStatus::Zombie:   return theme::Danger();
        default:                               return theme::TextMuted();
    }
}

} // namespace

void ProcessTab::Refresh() {
    std::vector<sysinfo::ProcessInfo> fresh = sysinfo::GetProcesses();
    std::unordered_map<uint32_t, sysinfo::ProcessInfo> fresh_by_pid;
    fresh_by_pid.reserve(fresh.size() * 2);
    for (auto& p : fresh) fresh_by_pid[p.pid] = p;

    if (!first_refresh_ && logs_) {
        for (auto& p : fresh) {
            if (prev_by_pid_.find(p.pid) == prev_by_pid_.end()) {
                logs_->AddEvent(LogsTab::Level::Success,
                                 "Process started: " + p.name + " (PID " + std::to_string(p.pid) + ")");
            }
        }
        for (auto& kv : prev_by_pid_) {
            if (fresh_by_pid.find(kv.first) == fresh_by_pid.end()) {
                logs_->AddEvent(LogsTab::Level::Error,
                                 "Process exited: " + kv.second.name + " (PID " + std::to_string(kv.first) + ")");
            }
        }
    }

    prev_by_pid_ = std::move(fresh_by_pid);
    processes_ = std::move(fresh);
    first_refresh_ = false;

    if (selected_pid_ >= 0 && prev_by_pid_.find((uint32_t)selected_pid_) == prev_by_pid_.end())
        selected_pid_ = -1;

    SortProcesses();
}

void ProcessTab::SortProcesses() {
    if (!cached_sort_specs_ || cached_sort_specs_->SpecsCount == 0) return;
    const ImGuiTableColumnSortSpecs& spec = cached_sort_specs_->Specs[0];
    bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;

    std::sort(processes_.begin(), processes_.end(),
              [&](const sysinfo::ProcessInfo& a, const sysinfo::ProcessInfo& b) {
        bool less;
        switch (spec.ColumnUserID) {
            case Col_Pid:    less = a.pid < b.pid; break;
            case Col_Cpu:    less = a.cpu_percent < b.cpu_percent; break;
            case Col_Mem:    less = a.memory_bytes < b.memory_bytes; break;
            case Col_Status: less = (int)a.status < (int)b.status; break;
            case Col_User:   less = ToLower(a.user) < ToLower(b.user); break;
            case Col_Name:
            default:         less = ToLower(a.name) < ToLower(b.name); break;
        }
        return ascending ? less : !less;
    });
}

void ProcessTab::RequestKill(uint32_t pid, const std::string& name, bool tree) {
    pending_confirm_ = true;
    pending_pid_ = pid;
    pending_name_ = name;
    pending_tree_ = tree;
}

void ProcessTab::KillSelected() {
    if (selected_pid_ < 0) return;
    auto it = prev_by_pid_.find((uint32_t)selected_pid_);
    std::string name = (it != prev_by_pid_.end()) ? it->second.name : "process";
    RequestKill((uint32_t)selected_pid_, name, /*tree=*/false);
}

void ProcessTab::Draw() {
    // --- Search bar ---
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::PanelAltColor());
    if (focus_search_next_frame_) {
        ImGui::SetKeyboardFocusHere();
        focus_search_next_frame_ = false;
    }
    ImGui::SetNextItemWidth(320);
    ImGui::InputTextWithHint("##search", "Search by name, PID or user... (Ctrl+F)",
                              search_buf_, sizeof(search_buf_));
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::TextColored(theme::TextMuted(), "%d processes", (int)processes_.size());

    if (selected_pid_ >= 0) {
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 220);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(theme::Danger().x, theme::Danger().y, theme::Danger().z, 0.25f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(theme::Danger().x, theme::Danger().y, theme::Danger().z, 0.5f));
        if (ImGui::Button("End Task (Ctrl+K)", ImVec2(200, 0))) KillSelected();
        ImGui::PopStyleColor(2);
    }

    ImGui::Separator();
    DrawTable();

    // --- Confirmation popup ---
    if (pending_confirm_) {
        ImGui::OpenPopup("Confirm End Process");
        pending_confirm_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(380, 0));
    if (ImGui::BeginPopupModal("Confirm End Process", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::TextWrapped("End %s \"%s\" (PID %u)? Unsaved data may be lost.",
                            pending_tree_ ? "process tree for" : "process",
                            pending_name_.c_str(), pending_pid_);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float w = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("Cancel", ImVec2(w * 0.48f, 0))) ImGui::CloseCurrentPopup();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, theme::Danger());
        if (ImGui::Button(pending_tree_ ? "End Process Tree" : "End Task", ImVec2(w * 0.48f, 0))) {
            bool ok = pending_tree_ ? sysinfo::KillProcessTree(pending_pid_)
                                     : sysinfo::KillProcess(pending_pid_);
            if (logs_) {
                logs_->AddEvent(ok ? LogsTab::Level::Warn : LogsTab::Level::Error,
                                 std::string(ok ? "Terminated " : "Failed to terminate ") +
                                 pending_name_ + " (PID " + std::to_string(pending_pid_) + ")" +
                                 (pending_tree_ ? " [tree]" : ""));
            }
            if ((int32_t)pending_pid_ == selected_pid_) selected_pid_ = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }
}

void ProcessTab::DrawTable() {
    static ImGuiTableFlags flags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Sortable |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp;

    if (!ImGui::BeginTable("##process_table", 6, flags, ImGui::GetContentRegionAvail())) return;

    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 2.4f, Col_Name);
    ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 70.0f, Col_Pid);
    ImGui::TableSetupColumn("CPU %", ImGuiTableColumnFlags_WidthFixed, 80.0f, Col_Cpu);
    ImGui::TableSetupColumn("Memory", ImGuiTableColumnFlags_WidthFixed, 100.0f, Col_Mem);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 90.0f, Col_Status);
    ImGui::TableSetupColumn("User", ImGuiTableColumnFlags_WidthStretch, 1.2f, Col_User);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs()) {
        cached_sort_specs_ = specs;
        if (specs->SpecsDirty) {
            SortProcesses();
            specs->SpecsDirty = false;
        }
    }

    std::string needle = ToLower(search_buf_);

    for (const sysinfo::ProcessInfo& p : processes_) {
        if (!needle.empty()) {
            bool match = ToLower(p.name).find(needle) != std::string::npos ||
                         ToLower(p.user).find(needle) != std::string::npos ||
                         std::to_string(p.pid).find(needle) != std::string::npos;
            if (!match) continue;
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        bool selected = ((int32_t)p.pid == selected_pid_);
        ImGui::PushID((int)p.pid);
        if (ImGui::Selectable(p.name.c_str(), selected,
                               ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
            selected_pid_ = (int32_t)p.pid;
        }
        if (ImGui::BeginPopupContextItem("row_ctx")) {
            selected_pid_ = (int32_t)p.pid;
            ImGui::TextColored(theme::TextMuted(), "%s (PID %u)", p.name.c_str(), p.pid);
            ImGui::Separator();
            if (ImGui::MenuItem("End Task")) RequestKill(p.pid, p.name, false);
            if (ImGui::MenuItem("End Process Tree")) RequestKill(p.pid, p.name, true);
            ImGui::EndPopup();
        }

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(std::to_string(p.pid).c_str());

        ImGui::TableNextColumn();
        ImVec4 cpu_col = p.cpu_percent > 60.0 ? theme::Danger()
                        : p.cpu_percent > 25.0 ? theme::Warning()
                        : theme::TextPrimary();
        ImGui::TextColored(cpu_col, "%.1f", p.cpu_percent);

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(sysinfo::FormatBytes(p.memory_bytes).c_str());

        ImGui::TableNextColumn();
        ImGui::TextColored(StatusColor(p.status), "%s", sysinfo::ProcessStatusToString(p.status));

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(p.user.c_str());

        ImGui::PopID();
    }

    ImGui::EndTable();
}

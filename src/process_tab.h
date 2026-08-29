#pragma once

#include <unordered_map>
#include <vector>

#include "imgui.h"
#include "system_info.h"

class LogsTab;

// "Processes" tab: live sortable/filterable process table with
// End Task / End Process Tree actions.
class ProcessTab {
public:
    explicit ProcessTab(LogsTab* logs) : logs_(logs) {}

    // Pulls a fresh process snapshot, diffs against the previous one to
    // detect start/stop events (logged to LogsTab), and re-sorts.
    void Refresh();

    void Draw();

    void FocusSearch() { focus_search_next_frame_ = true; }
    void KillSelected();

    int Count() const { return (int)processes_.size(); }

private:
    void DrawTable();
    void SortProcesses();
    void RequestKill(uint32_t pid, const std::string& name, bool tree);

    LogsTab* logs_ = nullptr;

    std::vector<sysinfo::ProcessInfo> processes_;
    std::unordered_map<uint32_t, sysinfo::ProcessInfo> prev_by_pid_;
    bool first_refresh_ = true;

    char search_buf_[128] = {};
    bool focus_search_next_frame_ = false;

    int32_t selected_pid_ = -1;
    ImGuiTableSortSpecs* cached_sort_specs_ = nullptr;

    // Pending confirmation popup state
    bool pending_confirm_ = false;
    uint32_t pending_pid_ = 0;
    std::string pending_name_;
    bool pending_tree_ = false;
};

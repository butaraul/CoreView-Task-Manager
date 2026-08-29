#pragma once

#include <deque>
#include <string>

// Rolling log of process lifecycle and system events, shown in the Logs tab.
// Other tabs (ProcessTab, App) push entries into this as things happen.
class LogsTab {
public:
    enum class Level { Info, Success, Warn, Error };

    struct Entry {
        std::string timestamp;
        std::string message;
        Level level;
    };

    LogsTab();

    void AddEvent(Level level, const std::string& message);
    void Draw();

    void FocusFilter() { focus_filter_next_frame_ = true; }

private:
    std::deque<Entry> entries_;
    static constexpr size_t kMaxEntries = 2000;

    char filter_buf_[128] = {};
    bool show_info_ = true;
    bool show_success_ = true;
    bool show_warn_ = true;
    bool show_error_ = true;
    bool auto_scroll_ = true;
    bool focus_filter_next_frame_ = false;
};

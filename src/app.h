#pragma once

struct GLFWwindow;

#include "logs_tab.h"
#include "performance_tab.h"
#include "process_tab.h"
#include "system_info.h"

// Owns the window/render backend and drives the main loop: global 1s
// refresh tick, keyboard shortcuts, tab bar, and the bottom status bar.
class App {
public:
    App();
    ~App();

    bool Init();
    void Run();
    void Shutdown();

private:
    enum class Tab { Processes, Performance, Logs };

    void BeginFrame();
    void DrawUI();
    void DrawStatusBar();
    void HandleShortcuts();
    void ForceRefresh();

    GLFWwindow* window_ = nullptr;

    Tab active_tab_ = Tab::Processes;

    LogsTab logs_;
    ProcessTab processes_{&logs_};
    PerformanceTab performance_;

    double last_refresh_time_ = 0.0;
    static constexpr double kRefreshIntervalSeconds = 1.0;

    // Cached for the status bar so it doesn't depend on which tab is active.
    sysinfo::CpuSample last_cpu_{};
    sysinfo::MemSample last_mem_{};
};

#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Cross-platform system/process introspection interface.
// Implemented once per OS in src/platform/{windows,linux,macos}.cpp.
// OS-agnostic formatting helpers live in system_info.cpp.
namespace sysinfo {

enum class ProcessStatus {
    Running,
    Sleeping,
    Stopped,
    Zombie,
    Unknown
};

struct ProcessInfo {
    uint32_t pid = 0;
    uint32_t ppid = 0;
    std::string name;
    double cpu_percent = 0.0;
    uint64_t memory_bytes = 0;
    ProcessStatus status = ProcessStatus::Unknown;
    std::string user;
};

struct CpuSample {
    double overall_percent = 0.0;
    std::vector<double> per_core_percent;
};

struct MemSample {
    uint64_t used_bytes = 0;
    uint64_t total_bytes = 0;
};

struct DiskSample {
    double read_bytes_per_sec = 0.0;
    double write_bytes_per_sec = 0.0;
};

struct NetSample {
    double recv_bytes_per_sec = 0.0;
    double send_bytes_per_sec = 0.0;
};

// ---- Implemented per-platform ----
std::vector<ProcessInfo> GetProcesses();
CpuSample GetCpuSample();
MemSample GetMemSample();
DiskSample GetDiskSample();
NetSample GetNetSample();
double GetUptimeSeconds();
bool KillProcess(uint32_t pid);
bool KillProcessTree(uint32_t pid);

// ---- OS-agnostic helpers (system_info.cpp) ----
std::string FormatBytes(uint64_t bytes);
std::string FormatRate(double bytes_per_sec);
std::string FormatUptime(double seconds);
const char* ProcessStatusToString(ProcessStatus status);

} // namespace sysinfo

# CoreView Task Manager

A cross-platform desktop task manager built with C++17 and [Dear ImGui]. Graphite-slate UI, teal/steel-blue accents, rounded geometry, and a soft hand-rolled glow — a reimagining of the system monitor, not a clone of one.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Built with C++](https://img.shields.io/badge/Built%20with-C%2B%2B-00599C.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey.svg)]()
[![UI](https://img.shields.io/badge/UI-Dear%20ImGui-ff69b4.svg)](https://github.com/ocornut/imgui)
[![Status](https://img.shields.io/badge/Status-Active-brightgreen.svg)]()
[![GitHub stars](https://img.shields.io/github/stars/butaraul/CoreView.svg?style=social)](https://github.com/butaraul/CoreView/stargazers)

## Features

- **Processes tab** — live, sortable, filterable process table (Name, PID, CPU %, Memory, Status, User). Search by name/PID/user, click any column header to sort, right-click a row for **End Task** / **End Process Tree**, both behind a confirmation dialog. Refreshes every second.
- **Performance tab** — compact scrollable dashboard:
  - CPU: overall usage graph + per-core meter bars
  - Memory: used/total with a graph
  - Disk: read/write throughput with graphs
  - Network: send/receive throughput with graphs
  - System uptime
- **Logs tab** — rolling event log of process start/stop events and kill actions, color-coded by level, filterable and searchable.
- **Status bar** — live process count, CPU load, memory usage, and the CoreView watermark.
- **Keyboard-first**: every core action has a shortcut (see below).
- **Native performance data on every OS** — no polling libraries, no shell-outs: Windows uses `tlhelp32` / `psapi` / `PDH`, Linux reads `/proc` directly, macOS uses `libproc`, Mach host APIs, and IOKit.

## Visual identity

CoreView trades the usual gray-and-blue task-manager look for a graphite-slate canvas (`#0A0D12`), signal-teal (`#2FD9C4`) and steel-blue (`#4FA8FF`) accents, generously rounded corners, and a lightweight glow effect drawn behind cards and the active tab — Dear ImGui has no native blur/shadow, so it's faked with nested translucent rounded rectangles of falling opacity (see `src/ui_theme.cpp`).

## Project structure

```
taskmanager/
├── src/
│   ├── main.cpp              # entry point
│   ├── app.cpp / app.h       # window, main loop, shortcuts, tab bar, status bar
│   ├── process_tab.cpp/h     # Processes tab
│   ├── performance_tab.cpp/h # Performance tab
│   ├── logs_tab.cpp/h        # Logs tab
│   ├── system_info.cpp/h     # cross-platform data interface + formatting helpers
│   ├── ui_theme.cpp/h        # CoreView palette, style, glow effect, meter bars
│   └── platform/
│       ├── windows.cpp       # tlhelp32 / psapi / PDH
│       ├── linux.cpp         # /proc
│       └── macos.cpp         # libproc / Mach host APIs / IOKit
├── CMakeLists.txt
└── README.md
```

## Dependencies

- CMake ≥ 3.16 and a C++17 compiler
- [GLFW](https://www.glfw.org/) — window/input backend (auto-fetched via CMake `FetchContent` if not found on the system)
- [Dear ImGui](https://github.com/ocornut/imgui) — fetched automatically at configure time, no manual vendoring needed
- OpenGL (system-provided on all three platforms)
- Windows only: `pdh.lib`, `psapi.lib` (both ship with the Windows SDK)
- Linux only: an OpenGL/GLX dev package (e.g. `libgl1-mesa-dev`) and X11/Wayland dev headers for GLFW if it needs to be fetched and built from source

On macOS, installing GLFW via Homebrew (`brew install glfw`) lets CMake find it without a network fetch. Dear ImGui is always fetched from source since it ships no CMake config of its own.

## Building

```bash
git clone <this-repo>
cd taskmanager
mkdir build && cd build
cmake ..
make -j
./CoreViewTaskManager        # CoreViewTaskManager.exe on Windows
```

On Windows with Visual Studio:

```bash
cmake -S . -B build
cmake --build build --config Release
```

The binary is named `CoreViewTaskManager` (`CoreViewTaskManager.exe` on Windows).

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+F` | Jump to Processes tab and focus the search box |
| `Ctrl+K` | End the selected task |
| `Ctrl+R` | Force an immediate refresh of process list and performance samples |
| `Ctrl+Q` | Quit |
| `Tab` | Cycle between Processes / Performance / Logs |

On macOS, `Cmd` works as an alias for `Ctrl` in all of the above.

## License

MIT — see [LICENSE](LICENSE).

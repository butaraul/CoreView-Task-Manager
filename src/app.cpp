#include "app.h"

#include <cstdio>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ui_theme.h"

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#elif defined(_WIN32)
#include <windows.h>
#include <GL/gl.h>
#else
#include <GL/gl.h>
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace {

void GlfwErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "[GLFW error %d] %s\n", error, description);
}

const char* kTabNames[] = {"Processes", "Performance", "Logs"};
constexpr int kTabCount = 3;
constexpr float kStatusBarHeight = 30.0f;

} // namespace

App::App() = default;
App::~App() { Shutdown(); }

bool App::Init() {
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit()) return false;

#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    const char* glsl_version = "#version 150";
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    const char* glsl_version = "#version 130";
#endif

    window_ = glfwCreateWindow(1280, 800, "CoreView Task Manager", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // CoreView is a live monitor, not a layout to persist

    theme::ApplyCoreViewStyle();

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ForceRefresh();
    return true;
}

void App::ForceRefresh() {
    processes_.Refresh();
    performance_.Refresh();
    last_cpu_ = sysinfo::GetCpuSample();
    last_mem_ = sysinfo::GetMemSample();
    last_refresh_time_ = glfwGetTime();
}

void App::HandleShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    bool mod = io.KeyCtrl || io.KeySuper;

    if (mod && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        active_tab_ = Tab::Processes;
        processes_.FocusSearch();
    }
    if (mod && ImGui::IsKeyPressed(ImGuiKey_K, false)) {
        active_tab_ = Tab::Processes;
        processes_.KillSelected();
    }
    if (mod && ImGui::IsKeyPressed(ImGuiKey_R, false)) {
        ForceRefresh();
    }
    if (mod && ImGui::IsKeyPressed(ImGuiKey_Q, false)) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
    if (!mod && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
        active_tab_ = static_cast<Tab>((static_cast<int>(active_tab_) + 1) % kTabCount);
    }
}

void App::BeginFrame() {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void App::DrawStatusBar() {
    ImVec4 accent = theme::AccentPrimary();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::PanelColor());
    ImGui::BeginChild("##status_bar", ImVec2(0, kStatusBarHeight), ImGuiChildFlags_Border);

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(theme::TextMuted(), "Processes:");
    ImGui::SameLine();
    ImGui::TextColored(theme::TextPrimary(), "%d", processes_.Count());

    ImGui::SameLine(0, 24);
    ImGui::TextColored(theme::TextMuted(), "CPU:");
    ImGui::SameLine();
    ImVec4 cpu_col = last_cpu_.overall_percent > 80.0 ? theme::Danger()
                    : last_cpu_.overall_percent > 50.0 ? theme::Warning() : theme::Success();
    ImGui::TextColored(cpu_col, "%.1f%%", last_cpu_.overall_percent);

    ImGui::SameLine(0, 24);
    ImGui::TextColored(theme::TextMuted(), "Memory:");
    ImGui::SameLine();
    double mem_pct = last_mem_.total_bytes > 0
        ? 100.0 * (double)last_mem_.used_bytes / (double)last_mem_.total_bytes : 0.0;
    ImVec4 mem_col = mem_pct > 85.0 ? theme::Danger() : mem_pct > 60.0 ? theme::Warning() : theme::Success();
    ImGui::TextColored(mem_col, "%.0f%%", mem_pct);
    ImGui::SameLine();
    ImGui::TextColored(theme::TextMuted(), "(%s / %s)",
                        sysinfo::FormatBytes(last_mem_.used_bytes).c_str(),
                        sysinfo::FormatBytes(last_mem_.total_bytes).c_str());

    const char* watermark = "CoreView";
    ImVec2 wm_size = ImGui::CalcTextSize(watermark);
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - wm_size.x - 16.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, accent);
    ImGui::TextUnformatted(watermark);
    ImGui::PopStyleColor();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void App::DrawUI() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                              ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 14));
    ImGui::Begin("##coreview_root", nullptr, flags);
    ImGui::PopStyleVar(2);

    // --- Header / tab bar ---
    ImGui::PushStyleColor(ImGuiCol_Text, theme::AccentPrimary());
    ImGui::Text("CoreView");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextColored(theme::TextMuted(), " Task Manager");

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 420);
    for (int i = 0; i < kTabCount; ++i) {
        bool selected = (static_cast<int>(active_tab_) == i);
        ImGui::PushID(i);
        if (selected) {
            ImVec2 min = ImGui::GetCursorScreenPos();
            ImVec2 sz = ImGui::CalcTextSize(kTabNames[i]);
            sz.x += ImGui::GetStyle().FramePadding.x * 2;
            sz.y += ImGui::GetStyle().FramePadding.y * 2;
            theme::GlowCard(min, ImVec2(min.x + sz.x, min.y + sz.y), 8.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(theme::AccentPrimary().x, theme::AccentPrimary().y, theme::AccentPrimary().z, 0.55f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        }
        if (ImGui::Button(kTabNames[i])) active_tab_ = static_cast<Tab>(i);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PopID();
    }
    ImGui::NewLine();
    ImGui::Separator();
    ImGui::Spacing();

    float content_height = ImGui::GetContentRegionAvail().y - kStatusBarHeight - ImGui::GetStyle().ItemSpacing.y;
    ImGui::BeginChild("##tab_content", ImVec2(0, content_height), ImGuiChildFlags_None);
    switch (active_tab_) {
        case Tab::Processes:  processes_.Draw();  break;
        case Tab::Performance: performance_.Draw(); break;
        case Tab::Logs:        logs_.Draw();        break;
    }
    ImGui::EndChild();

    DrawStatusBar();

    ImGui::End();
}

void App::Run() {
    while (!glfwWindowShouldClose(window_)) {
        BeginFrame();
        HandleShortcuts();

        double now = glfwGetTime();
        if (now - last_refresh_time_ >= kRefreshIntervalSeconds) {
            processes_.Refresh();
            performance_.Refresh();
            last_cpu_ = sysinfo::GetCpuSample();
            last_mem_ = sysinfo::GetMemSample();
            last_refresh_time_ = now;
        }

        ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::BackgroundColor());
        DrawUI();
        ImGui::PopStyleColor();

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window_, &w, &h);
        glViewport(0, 0, w, h);
        ImVec4 bg = theme::BackgroundColor();
        glClearColor(bg.x, bg.y, bg.z, bg.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window_);
    }
}

void App::Shutdown() {
    if (!window_) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window_);
    glfwTerminate();
    window_ = nullptr;
}

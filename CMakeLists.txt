cmake_minimum_required(VERSION 3.16)
project(CoreViewTaskManager CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

# --------------------------------------------------------------------------
# GLFW - prefer a system/package-manager install, fetch as a fallback.
# --------------------------------------------------------------------------
find_package(glfw3 3.3 QUIET)
if(NOT glfw3_FOUND)
    message(STATUS "System GLFW not found, fetching GLFW 3.4 from source")
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glfw)
endif()

# --------------------------------------------------------------------------
# Dear ImGui (immediate mode GUI, no CMake support upstream - build manually)
# --------------------------------------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)

find_package(OpenGL REQUIRED)

# --------------------------------------------------------------------------
# Platform backend selection
# --------------------------------------------------------------------------
if(WIN32)
    set(PLATFORM_SOURCES src/platform/windows.cpp)
elseif(APPLE)
    set(PLATFORM_SOURCES src/platform/macos.cpp)
else()
    set(PLATFORM_SOURCES src/platform/linux.cpp)
endif()

add_executable(coreview_task_manager
    src/main.cpp
    src/app.cpp
    src/process_tab.cpp
    src/performance_tab.cpp
    src/logs_tab.cpp
    src/system_info.cpp
    src/ui_theme.cpp
    ${PLATFORM_SOURCES}
)

target_include_directories(coreview_task_manager PRIVATE src)
target_link_libraries(coreview_task_manager PRIVATE imgui OpenGL::GL)

if(APPLE)
    target_link_libraries(coreview_task_manager PRIVATE
        "-framework IOKit"
        "-framework CoreFoundation"
    )
elseif(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(coreview_task_manager PRIVATE Threads::Threads)
elseif(WIN32)
    target_link_libraries(coreview_task_manager PRIVATE pdh psapi)
endif()

set_target_properties(coreview_task_manager PROPERTIES OUTPUT_NAME "CoreViewTaskManager")

if(MSVC)
    target_compile_options(coreview_task_manager PRIVATE /W4)
else()
    target_compile_options(coreview_task_manager PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

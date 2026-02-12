# ============================================================================
# Use dependencies' native CMake configuration when available
# ============================================================================

# fmt - formatting library (required by spdlog)
set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
set(FMT_TEST OFF CACHE BOOL "" FORCE)
set(FMT_DOC OFF CACHE BOOL "" FORCE)
add_subdirectory(fmt)

# spdlog - use its own CMake configuration with external fmt
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "" FORCE)
add_subdirectory(spdlog)

# stduuid - use its own CMake configuration
set(UUID_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(UUID_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(stduuid)

# doctest - use its own CMake configuration
set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB ON CACHE BOOL "" FORCE)
set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
add_subdirectory(doctest)

# sol2 - use its own CMake configuration (header-only, for lua binding)
set(SOL2_BUILD_LUA OFF CACHE BOOL "" FORCE)
set(SOL2_TESTS OFF CACHE BOOL "" FORCE)
set(SOL2_CI OFF CACHE BOOL "" FORCE)
add_subdirectory(sol2)

# pybind11 - use its own CMake configuration (for python binding)
if(NOT PYTHON_EXECUTABLE STREQUAL "no")
    set(PYBIND11_INSTALL OFF CACHE BOOL "" FORCE)
    set(PYBIND11_TEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(pybind11)
endif()

# nativefiledialog-extended - use its own CMake configuration
set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NFD_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(nativefiledialog-extended)

# ============================================================================
# Dependencies without native CMake configuration - build manually
# ============================================================================

# ImGui (no built-in CMake)
add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/misc/cpp/imgui_stdlib.cpp
)

target_include_directories(imgui PUBLIC
    imgui
    imgui/backends
    imgui/misc/cpp
)

# Backend specific files for ImGui
if(BACKEND STREQUAL "dx11")
    target_sources(imgui PRIVATE
        imgui/backends/imgui_impl_win32.cpp
        imgui/backends/imgui_impl_dx11.cpp
    )
    target_link_libraries(imgui PUBLIC d3d11 dxgi d3dcompiler)
elseif(BACKEND STREQUAL "dx12")
    target_sources(imgui PRIVATE
        imgui/backends/imgui_impl_win32.cpp
        imgui/backends/imgui_impl_dx12.cpp
    )
    target_link_libraries(imgui PUBLIC d3d12 dxgi d3dcompiler)
elseif(BACKEND STREQUAL "vulkan")
    target_sources(imgui PRIVATE
        imgui/backends/imgui_impl_glfw.cpp
        imgui/backends/imgui_impl_vulkan.cpp
    )
    if(VULKAN_SDK)
        target_include_directories(imgui PUBLIC ${VULKAN_SDK}/Include)
        target_link_directories(imgui PUBLIC ${VULKAN_SDK}/Lib)
    endif()
    if(WIN32)
        target_link_libraries(imgui PUBLIC glfw vulkan-1)
    else()
        target_link_libraries(imgui PUBLIC glfw vulkan)
    endif()
elseif(BACKEND STREQUAL "gl2")
    target_sources(imgui PRIVATE
        imgui/backends/imgui_impl_glfw.cpp
        imgui/backends/imgui_impl_opengl2.cpp
    )
elseif(BACKEND STREQUAL "gl3")
    target_sources(imgui PRIVATE
        imgui/backends/imgui_impl_glfw.cpp
        imgui/backends/imgui_impl_opengl3.cpp
    )
endif()

# Platform specific libraries for ImGui
if(WIN32)
    target_link_libraries(imgui PUBLIC ole32 uuid gdi32 comctl32 dwmapi)
    if(BACKEND STREQUAL "gl2" OR BACKEND STREQUAL "gl3")
        target_link_libraries(imgui PUBLIC glfw3 opengl32)
    elseif(BACKEND STREQUAL "vulkan")
        target_link_libraries(imgui PUBLIC glfw3)
    endif()
else()
    target_link_libraries(imgui PUBLIC glfw dl pthread)
    if(APPLE)
        target_link_libraries(imgui PUBLIC "-framework OpenGL")
    else()
        target_link_libraries(imgui PUBLIC GL)
    endif()
endif()

# boxer - manual configuration (its CMake config is too old)
add_library(boxer STATIC)

if(APPLE)
    target_sources(boxer PRIVATE boxer/src/boxer_osx.mm)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    target_link_libraries(boxer PUBLIC ${COCOA_LIBRARY})
elseif(UNIX)
    target_sources(boxer PRIVATE boxer/src/boxer_zenity.cpp)
    # Note: zenity implementation uses system zenity, no extra dependencies
else()
    target_sources(boxer PRIVATE boxer/src/boxer_win.cpp)
endif()

target_include_directories(boxer PUBLIC boxer/include)

# s7 Scheme interpreter - no built-in CMake
add_library(s7 STATIC
    s7/s7.c
    ${CMAKE_SOURCE_DIR}/examples/ngs7/s7-extensions.cpp
)

target_include_directories(s7 PUBLIC s7 ${CMAKE_SOURCE_DIR}/examples/ngs7)

if(UNIX AND NOT APPLE)
    target_link_libraries(s7 PUBLIC dl)
endif()

# miniz - no built-in CMake
add_library(miniz STATIC miniz/miniz.c)
target_include_directories(miniz PUBLIC miniz)

# lua - no built-in CMake
add_library(lua STATIC
    lua/lapi.c
    lua/lauxlib.c
    lua/lbaselib.c
    lua/lcode.c
    lua/lcorolib.c
    lua/lctype.c
    lua/ldblib.c
    lua/ldebug.c
    lua/ldo.c
    lua/ldump.c
    lua/lfunc.c
    lua/lgc.c
    lua/linit.c
    lua/liolib.c
    lua/llex.c
    lua/lmathlib.c
    lua/lmem.c
    lua/loadlib.c
    lua/lobject.c
    lua/lopcodes.c
    lua/loslib.c
    lua/lparser.c
    lua/lstate.c
    lua/lstring.c
    lua/lstrlib.c
    lua/ltable.c
    lua/ltablib.c
    lua/ltm.c
    lua/lundump.c
    lua/lutf8lib.c
    lua/lvm.c
    lua/lzio.c
)

target_include_directories(lua PUBLIC lua)

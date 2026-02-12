cmake_minimum_required(VERSION 3.15)
project(nged VERSION 1.0.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Global definitions
add_definitions(-DSPDLOG_COMPILED_LIB=1)

# AddressSanitizer configuration
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    elseif(MSVC)
        add_compile_options(/fsanitize=address)
    endif()
endif()

# Build type specific settings
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
endif()

# Options
set(BACKEND "dx11" CACHE STRING "Renderer backend (dx11, dx12, vulkan, gl2, gl3)")
set_property(CACHE BACKEND PROPERTY STRINGS dx11 dx12 vulkan gl2 gl3)

if(WIN32)
    if(NOT DEFINED BACKEND)
        set(BACKEND "dx11")
    endif()
else()
    if(NOT DEFINED BACKEND)
        set(BACKEND "gl2")
    endif()
endif()

set(PYTHON_EXECUTABLE "no" CACHE STRING "Python executable path, or 'no' if python binding is not needed")
set(VULKAN_SDK "" CACHE PATH "Vulkan SDK path")

# Backend definitions
if(BACKEND STREQUAL "dx11")
    add_definitions(-DNGED_BACKEND_DX11)
elseif(BACKEND STREQUAL "dx12")
    add_definitions(-DNGED_BACKEND_DX12)
elseif(BACKEND STREQUAL "vulkan")
    add_definitions(-DNGED_BACKEND_VULKAN)
elseif(BACKEND STREQUAL "gl2")
    add_definitions(-DNGED_BACKEND_GL2)
elseif(BACKEND STREQUAL "gl3")
    add_definitions(-DNGED_BACKEND_GL3)
endif()

# Platform specific settings
if(APPLE)
    # Homebrew detection
    if(DEFINED ENV{HOMEBREW_PREFIX})
        include_directories($ENV{HOMEBREW_PREFIX}/include)
        link_directories($ENV{HOMEBREW_PREFIX}/lib)
    endif()
endif()

# Find packages
if(WIN32 AND (BACKEND STREQUAL "vulkan" OR BACKEND STREQUAL "gl2" OR BACKEND STREQUAL "gl3"))
    find_package(glfw3 REQUIRED)
endif()

if(BACKEND STREQUAL "vulkan" AND VULKAN_SDK)
    include_directories(${VULKAN_SDK}/Include)
    link_directories(${VULKAN_SDK}/Lib)
endif()

# Subdirectories
add_subdirectory(deps)
add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(tests)

# Python bindings
if(NOT PYTHON_EXECUTABLE STREQUAL "no")
    add_subdirectory(python)
endif()

# Install configuration
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

install(TARGETS nged ngdoc entry
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

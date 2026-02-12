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
set(CMAKE_CXX_FLAGS_ADDRESSSANITIZE "${CMAKE_CXX_FLAGS_DEBUG}" CACHE STRING "Flags used by the CXX compiler during AddressSanitize builds." FORCE)
set(CMAKE_C_FLAGS_ADDRESSSANITIZE "${CMAKE_C_FLAGS_DEBUG}" CACHE STRING "Flags used by the C compiler during AddressSanitize builds." FORCE)
set(CMAKE_EXE_LINKER_FLAGS_ADDRESSSANITIZE "${CMAKE_EXE_LINKER_FLAGS_DEBUG}" CACHE STRING "Flags used for linking binaries during AddressSanitize builds." FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_ADDRESSSANITIZE "${CMAKE_SHARED_LINKER_FLAGS_DEBUG}" CACHE STRING "Flags used by the shared libraries linker during AddressSanitize builds." FORCE)

if(CMAKE_CONFIGURATION_TYPES)
    list(APPEND CMAKE_CONFIGURATION_TYPES "AddressSanitize")
    list(REMOVE_DUPLICATES CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Semicolon separated list of supported configuration types." FORCE)
endif()

if(MSVC)
    add_compile_options($<$<CONFIG:AddressSanitize>:/fsanitize=address>)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options($<$<CONFIG:AddressSanitize>:-fsanitize=address> $<$<CONFIG:AddressSanitize>:-fno-omit-frame-pointer>)
    add_link_options($<$<CONFIG:AddressSanitize>:-fsanitize=address>)
endif()


# Build type specific settings
add_compile_definitions($<$<CONFIG:Debug>:DEBUG> $<$<CONFIG:AddressSanitize>:DEBUG>)


# Determine default backend
if(WIN32)
    set(DEFAULT_BACKEND "dx11")
elseif(APPLE)
    set(DEFAULT_BACKEND "gl3")
else()
    set(DEFAULT_BACKEND "gl2")
endif()

# Options
set(BACKEND "${DEFAULT_BACKEND}" CACHE STRING "Renderer backend (dx11, dx12, vulkan, gl2, gl3)")
set_property(CACHE BACKEND PROPERTY STRINGS dx11 dx12 vulkan gl2 gl3)

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
    add_definitions(-DGL_SILENCE_DEPRECATION)
endif()

# Find packages
if((WIN32 OR APPLE) AND (BACKEND STREQUAL "vulkan" OR BACKEND STREQUAL "gl2" OR BACKEND STREQUAL "gl3"))
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

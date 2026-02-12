# ngdoc library
add_library(ngdoc STATIC
    ngdoc.cpp
    ngdraw.cpp
    style.cpp
    ${CMAKE_SOURCE_DIR}/include/nged/ngdoc.h
    ${CMAKE_SOURCE_DIR}/include/nged/gmath.h
    ${CMAKE_SOURCE_DIR}/include/nged/style.h
    ${CMAKE_SOURCE_DIR}/include/nged/utils.h
)

target_include_directories(ngdoc PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/deps/nlohmann
    ${CMAKE_SOURCE_DIR}/deps/spdlog/include
    ${CMAKE_SOURCE_DIR}/deps/stduuid/include
    ${CMAKE_SOURCE_DIR}/deps/stduuid
    ${CMAKE_SOURCE_DIR}/deps/parallel_hashmap/parallel_hashmap
)

target_link_libraries(ngdoc PUBLIC spdlog miniz)

# entry library
add_library(entry STATIC
    entry/entry.cpp
    ${CMAKE_SOURCE_DIR}/include/nged/entry/entry.h
    ${CMAKE_SOURCE_DIR}/include/nged/entry/texture.h
)

if(BACKEND STREQUAL "dx11")
    target_sources(entry PRIVATE
        entry/dx11_main.cpp
        entry/dx11_texture.cpp
    )
elseif(BACKEND STREQUAL "dx12")
    target_sources(entry PRIVATE
        entry/dx12_main.cpp
        entry/dx12_texture.cpp
    )
elseif(BACKEND STREQUAL "vulkan")
    target_sources(entry PRIVATE
        entry/vulkan_main.cpp
        entry/vulkan_texture.cpp
    )
elseif(BACKEND STREQUAL "gl2")
    target_sources(entry PRIVATE
        entry/gl2_main.cpp
        entry/gl_texture.cpp
    )
elseif(BACKEND STREQUAL "gl3")
    target_sources(entry PRIVATE
        entry/gl3_main.cpp
        entry/gl_texture.cpp
    )
elseif(BACKEND STREQUAL "metal")
    target_sources(entry PRIVATE
        entry/metal_main.mm
        entry/metal_texture.mm
    )
endif()

target_include_directories(entry PUBLIC
    ${CMAKE_SOURCE_DIR}/include/nged/entry
)

target_link_libraries(entry PUBLIC imgui)

if(WIN32)
    target_link_libraries(entry PUBLIC ws2_32 advapi32 gdi32 shell32 version)
    if(NOT BACKEND STREQUAL "dx11" AND NOT BACKEND STREQUAL "dx12")
        target_link_libraries(entry PUBLIC glfw3)
    endif()
    if(BACKEND STREQUAL "gl2" OR BACKEND STREQUAL "gl3")
        # Note: gl3w might need special handling
    endif()
else()
    target_link_libraries(entry PUBLIC iconv)
endif()

if(APPLE)
    target_link_libraries(entry PUBLIC
        "-framework AppKit"
        "-framework IOKit"
        "-framework QuartzCore"
    )
    if(BACKEND STREQUAL "metal")
        target_link_libraries(entry PUBLIC "-framework Metal")
    endif()
endif()

# nged library
add_library(nged STATIC
    nged.cpp
    nged_imgui.cpp
    nged_imgui_fonts.cpp
    ${CMAKE_SOURCE_DIR}/include/nged/nged.h
    ${CMAKE_SOURCE_DIR}/include/nged/nged_imgui.h
    ${CMAKE_SOURCE_DIR}/include/nged/ngpy.h
    ${CMAKE_SOURCE_DIR}/include/nged/pybind11_imgui.h
    ${CMAKE_SOURCE_DIR}/include/nged/res/fa_icondef.h
)

target_include_directories(nged PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/deps/boxer/include
    ${CMAKE_SOURCE_DIR}/deps/nlohmann
    ${CMAKE_SOURCE_DIR}/deps/spdlog/include
    ${CMAKE_SOURCE_DIR}/deps/parallel_hashmap/parallel_hashmap
    ${CMAKE_SOURCE_DIR}/include/nged/entry
)

target_link_libraries(nged PUBLIC
    spdlog::spdlog
    nfd
    imgui
    boxer
    ngdoc
    entry
)

if(MSVC)
    target_compile_options(nged PRIVATE /bigobj)
endif()
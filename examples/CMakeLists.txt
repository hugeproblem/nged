# demo executable
add_executable(demo
    demo/main.cpp
)

target_include_directories(demo PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/deps/boxer/include
)

target_link_libraries(demo PRIVATE nged entry)

# typed_demo executable
add_executable(typed_demo
    typed_demo/main.cpp
)

target_include_directories(typed_demo PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/deps/boxer/include
)

target_link_libraries(typed_demo PRIVATE nged entry)

# s7e executable
add_executable(s7e
    ngs7/s7e.cpp
)

target_link_libraries(s7e PRIVATE s7)

# ngs7 executable
add_executable(ngs7
    ngs7/ngs7.cpp
    ngs7/main.cpp
)

target_include_directories(ngs7 PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/deps/boxer/include
    ${CMAKE_SOURCE_DIR}/deps/imgui
    ${CMAKE_SOURCE_DIR}/deps/imgui/backends
    ${CMAKE_SOURCE_DIR}/deps/nlohmann
    ${CMAKE_SOURCE_DIR}/deps/spdlog/include
    ${CMAKE_SOURCE_DIR}/deps/subprocess.h
)

target_link_libraries(ngs7 PRIVATE nged s7 entry)

if(WIN32)
    target_sources(ngs7 PRIVATE ngs7/icon.rc)
endif()

# tests executable
add_executable(tests
    main.cpp
    graph_tests.cpp
    utils_tests.cpp
)

target_include_directories(tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/deps/doctest
)

target_link_libraries(tests PRIVATE ngdoc spdlog::spdlog)

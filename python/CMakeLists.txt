# Python bindings (optional)
if(NOT PYTHON_EXECUTABLE STREQUAL "no")
    find_package(Python COMPONENTS Interpreter Development REQUIRED)

    # parmscript library
    add_library(parmscript STATIC
        ${CMAKE_SOURCE_DIR}/deps/parmscript/inspectorext.cpp
        ${CMAKE_SOURCE_DIR}/deps/parmscript/jsonparm.cpp
        ${CMAKE_SOURCE_DIR}/deps/parmscript/parminspector.cpp
        ${CMAKE_SOURCE_DIR}/deps/parmscript/parmscript.cpp
        ${CMAKE_SOURCE_DIR}/deps/parmscript/pyparm.cpp
    )

    target_include_directories(parmscript PUBLIC
        ${CMAKE_SOURCE_DIR}/deps/lua
        ${CMAKE_SOURCE_DIR}/deps/sol2/include
        ${CMAKE_SOURCE_DIR}/deps/nlohmann
        ${CMAKE_SOURCE_DIR}/deps/pybind11/include
        ${Python_INCLUDE_DIRS}
    )

    target_link_libraries(parmscript PUBLIC lua imgui nfd sol2::sol2 pybind11::headers)

    # ngpy Python extension
    add_library(ngpy SHARED
        ${CMAKE_SOURCE_DIR}/src/ngpy.cpp
        ${CMAKE_SOURCE_DIR}/src/pybind11_imgui.cpp
    )

    target_include_directories(ngpy PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/deps/boxer/include
        ${CMAKE_SOURCE_DIR}/deps/imgui
        ${CMAKE_SOURCE_DIR}/deps/nlohmann
        ${CMAKE_SOURCE_DIR}/deps/spdlog/include
        ${CMAKE_SOURCE_DIR}/deps/parallel_hashmap/parallel_hashmap
        ${CMAKE_SOURCE_DIR}/deps/subprocess.h
        ${CMAKE_SOURCE_DIR}/deps/parmscript
        ${CMAKE_SOURCE_DIR}/deps/pybind11/include
        ${Python_INCLUDE_DIRS}
    )

    target_link_libraries(ngpy PRIVATE
        nged
        entry
        parmscript
        pybind11::headers
        ${Python_LIBRARIES}
    )

    if(MSVC)
        target_compile_options(ngpy PRIVATE /bigobj)
    endif()

    # Set Python extension properties
    set_target_properties(ngpy PROPERTIES
        PREFIX ""
        SUFFIX "${Python_SOABI}${CMAKE_SHARED_LIBRARY_SUFFIX}"
    )

    if(APPLE)
        set_target_properties(ngpy PROPERTIES
            LINK_FLAGS "-undefined dynamic_lookup"
        )
    endif()
endif()

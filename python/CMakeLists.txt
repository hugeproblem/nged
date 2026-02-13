# Python bindings (optional)
if(NOT PYTHON_EXECUTABLE STREQUAL "no")
    find_package(Python COMPONENTS Interpreter Development REQUIRED)

    # Generate parmexpr.lua.h
    set(PARMEXPR_LUA "${CMAKE_SOURCE_DIR}/deps/parmscript/parmexpr.lua")
    set(PARMEXPR_HEADER "${CMAKE_CURRENT_BINARY_DIR}/parmexpr.lua.h")

    add_custom_command(
        OUTPUT ${PARMEXPR_HEADER}
        COMMAND ${Python_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/bin2c.py" "${PARMEXPR_LUA}" "${PARMEXPR_HEADER}"
        DEPENDS "${PARMEXPR_LUA}" "${CMAKE_CURRENT_SOURCE_DIR}/bin2c.py"
        COMMENT "Generating parmexpr.lua.h"
    )

    # parmscript library
    add_library(parmscript STATIC
        ${PARMEXPR_HEADER}
        ${CMAKE_SOURCE_DIR}/deps/parmscript/inspectorext.cpp
        ${CMAKE_SOURCE_DIR}/deps/parmscript/jsonparm.cpp
        ${CMAKE_SOURCE_DIR}/deps/parmscript/parminspector.cpp
        ${CMAKE_SOURCE_DIR}/deps/parmscript/parmscript.cpp
        ${CMAKE_SOURCE_DIR}/deps/parmscript/pyparm.cpp
    )

    target_include_directories(parmscript PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/deps/lua
        ${CMAKE_SOURCE_DIR}/deps/sol2/include
        ${CMAKE_SOURCE_DIR}/deps/nlohmann
        ${CMAKE_SOURCE_DIR}/deps/pybind11/include
        ${Python_INCLUDE_DIRS}
    )

    target_link_libraries(parmscript PUBLIC lua imgui nfd sol2::sol2 pybind11::headers)

    # ngpy Python extension
    pybind11_add_module(ngpy
        ${CMAKE_SOURCE_DIR}/src/ngpy.cpp
        ${CMAKE_SOURCE_DIR}/src/pybind11_imgui.cpp
    )

    # Output as a package: ngpy/__init__.pyd
    set_target_properties(ngpy PROPERTIES OUTPUT_NAME "__init__")
    set_target_properties(ngpy PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ngpy")
    set_target_properties(ngpy PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ngpy")

    target_include_directories(ngpy PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/deps/boxer/include
        ${CMAKE_SOURCE_DIR}/deps/imgui
        ${CMAKE_SOURCE_DIR}/deps/nlohmann
        ${CMAKE_SOURCE_DIR}/deps/spdlog/include
        ${CMAKE_SOURCE_DIR}/deps/parallel_hashmap/parallel_hashmap
        ${CMAKE_SOURCE_DIR}/deps/subprocess.h
        ${CMAKE_SOURCE_DIR}/deps/parmscript
    )

    target_link_libraries(ngpy PRIVATE
        nged
        entry
        parmscript
    )

    if(MSVC)
        target_compile_options(ngpy PRIVATE /bigobj)
    endif()

    # Generate stubs after build
    # PYTHONPATH points to binary dir so 'import ngpy' finds 'ngpy/__init__.pyd'
    add_custom_command(TARGET ngpy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}" "${Python_EXECUTABLE}" -m pybind11_stubgen ngpy --output-dir "${CMAKE_CURRENT_BINARY_DIR}" --root-suffix "" --ignore-all-errors
        COMMENT "Generating Python stubs for ngpy"
        VERBATIM
    )

    # Install the Python extension module (to ngpy/__init__.pyd)
    install(TARGETS ngpy
        LIBRARY DESTINATION ngpy COMPONENT nged_python
        RUNTIME DESTINATION ngpy COMPONENT nged_python
    )

    # Install the generated stubs (merge into ngpy folder)
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ngpy"
        DESTINATION .
        COMPONENT nged_python
        FILES_MATCHING PATTERN "*.pyi"
    )
endif()

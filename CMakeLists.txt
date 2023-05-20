cmake_minimum_required(VERSION 3.5)

project(spin++ VERSION 1.0.0)

# Compiler settings
set(CMAKE_CXX_COMPILER g++)

# Compiler settings
add_compile_options(-g -O0 -DNXT -Wall -pedantic)

# Source files
file(GLOB MODELS_SOURCES
    "src/models/*.cpp" "src/models/*.hpp")
file(GLOB PANGEN_SOURCES
    "src/pangen/*.cpp" "src/pangen/*.hpp"
    "src/codegen/*.cpp" "src/codegen/*.hpp")
file(GLOB MAIN_SOURCES
    "src/main/*.cpp"
    "src/main/*.hpp")

file(GLOB LEXER_SOURCES
    "src/lexer/*.cpp"
    "src/lexer/*.hpp")

file(GLOB UTILS_SOURCES
    "src/utils/*.cpp" "src/utils/*.hpp"
    "src/utils/**/*.cpp" "src/utils/**/*.hpp"
    "src/fatal/*.hpp" "src/fatal/*.cpp")
file(GLOB PROCESSOR_SOURCES
    "src/**/*.cpp" "src/**/*.hpp")
file(GLOB TL_SOURCES "src/tl/*.cpp" "src/tl/*.hpp")

set(SPIN_SOURCES
    src/spin.hpp
    src/main.cpp
    src/version/version.hpp.in
)


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/version/version.hpp.in ${CMAKE_CURRENT_SOURCE_DIR}/src/version/version.hpp)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Bison settings
find_package(BISON REQUIRED)

if(BISON_FOUND)
    set(BISON_FLAGS "--defines=${CMAKE_CURRENT_BINARY_DIR}/y.tab.h")
    set(BISON_INPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/spin.yy")
    set(BISON_OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/y.tab.cpp")

    BISON_TARGET(SpinParser ${BISON_INPUT_FILE} ${BISON_OUTPUT_FILE}
             DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/y.tab.h
             VERBOSE)
else()
    message(FATAL_ERROR "Bison not found")
endif()

find_package(fmt REQUIRED)
include_directories(${fmt_INCLUDE_DIR})

add_executable(spin++
    ${SPIN_SOURCES} ${MODELS_SOURCES}
    ${PANGEN_SOURCES} ${UTILS_SOURCES}
    ${TL_SOURCES} ${MAIN_SOURCES} ${PROCESSOR_SOURCES}
    ${LEXER_SOURCES}  ${BISON_SpinParser_OUTPUTS})

set_target_properties(spin++ PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(spin++ PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(spin++ fmt::fmt)

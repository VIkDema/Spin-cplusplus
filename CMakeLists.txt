cmake_minimum_required(VERSION 3.5)

project(spin VERSION 1.0.0)

# Compiler settings
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-g -O0 -DNXT -Wall -pedantic")

# Additional flags for picky compilation
set(CMAKE_CXX_FLAGS_PICKY "-std=c++17 -Wstrict-prototypes -pedantic -fno-strength-reduce -fno-builtin -W -Wshadow -Wpointer-arith -Wcast-qual -Winline -Wall")

# Source files
set(SPIN_SOURCES
    src/spin.hpp
    src/spinlex.cpp
    src/sym.cpp
    src/vars.cpp
    src/main.cpp
    src/msc_tcl.cpp
    src/mesg.cpp
    src/flow.cpp
    src/sched.cpp
    src/run.cpp
    src/pangen/pangen1.cpp
    src/pangen/pangen2.cpp
    src/pangen/pangen3.cpp
    src/pangen/pangen4.cpp
    src/pangen/pangen5.cpp
    src/pangen/pangen6.cpp
    src/pangen/pangen7.cpp
    src/guided.cpp
    src/dstep.cpp
    src/structs.cpp
    src/reprosrc.cpp
    src/version/version.hpp.in
    src/fatal/fatal.hpp
    src/fatal/fatal.cpp
    src/utils/format/format.hpp
    src/utils/format/format.cpp
)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/version/version.hpp.in ${CMAKE_CURRENT_SOURCE_DIR}/src/version/version.hpp)

set(TL_SOURCES
    src/tl/tl_parse.cpp
    src/tl/tl_lex.cpp
    src/tl/tl_main.cpp
    src/tl/tl_trans.cpp
    src/tl/tl_buchi.cpp
    src/tl/tl_mem.cpp
    src/tl/tl_rewrt.cpp
    src/tl/tl_cache.cpp
)

# Bison settings
find_program(BISON_EXECUTABLE bison)

if(NOT BISON_EXECUTABLE)
    message(FATAL_ERROR "bison not found")
endif()

set(BISON_FLAGS "--defines=${CMAKE_CURRENT_BINARY_DIR}/y.tab.h")
set(BISON_INPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/spin.y")
set(BISON_OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/y.tab.cpp")

add_custom_command(
    OUTPUT ${BISON_OUTPUT_FILE} ${CMAKE_CURRENT_BINARY_DIR}/y.tab.h
    COMMAND ${BISON_EXECUTABLE} ${BISON_FLAGS} ${BISON_INPUT_FILE} -o ${BISON_OUTPUT_FILE}
    DEPENDS ${BISON_INPUT_FILE}
)

find_package(fmt REQUIRED)
include_directories(${fmt_INCLUDE_DIR})

# Build targets
add_executable(spin ${SPIN_SOURCES} ${TL_SOURCES} ${BISON_OUTPUT_FILE})
target_include_directories(spin PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(spin fmt::fmt)

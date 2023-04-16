cmake_minimum_required(VERSION 3.5)

project(spin)

# Compiler settings
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-g -O0 -DNXT -Wall -pedantic")

# Additional flags for picky compilation
set(CMAKE_CXX_FLAGS_PICKY "-std=c++17 -Wstrict-prototypes -pedantic -fno-strength-reduce -fno-builtin -W -Wshadow -Wpointer-arith -Wcast-qual -Winline -Wall")

# Source files
set(SPIN_SOURCES
    spin.hpp
    spinlex.cpp
    sym.cpp
    vars.cpp
    main.cpp
    msc_tcl.cpp
    mesg.cpp
    flow.cpp
    sched.cpp
    run.cpp
    pangen/pangen1.cpp
    pangen/pangen2.cpp
    pangen/pangen3.cpp
    pangen/pangen4.cpp
    pangen/pangen5.cpp
    pangen/pangen6.cpp
    pangen/pangen7.cpp
    guided.cpp
    dstep.cpp
    structs.cpp
    reprosrc.cpp
)

set(TL_SOURCES
    tl/tl_parse.cpp
    tl/tl_lex.cpp
    tl/tl_main.cpp
    tl/tl_trans.cpp
    tl/tl_buchi.cpp
    tl/tl_mem.cpp
    tl/tl_rewrt.cpp
    tl/tl_cache.cpp
)

# Bison settings
find_program(BISON_EXECUTABLE bison)

if(NOT BISON_EXECUTABLE)
    message(FATAL_ERROR "bison not found")
endif()

set(BISON_FLAGS "--defines=${CMAKE_CURRENT_BINARY_DIR}/y.tab.h")
set(BISON_INPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/spin.y")
set(BISON_OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/y.tab.cpp")

add_custom_command(
    OUTPUT ${BISON_OUTPUT_FILE} ${CMAKE_CURRENT_BINARY_DIR}/y.tab.h
    COMMAND ${BISON_EXECUTABLE} ${BISON_FLAGS} ${BISON_INPUT_FILE} -o ${BISON_OUTPUT_FILE}
    DEPENDS ${BISON_INPUT_FILE}
)

# Build targets
add_executable(spin ${SPIN_SOURCES} ${TL_SOURCES} ${BISON_OUTPUT_FILE})
target_include_directories(spin PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

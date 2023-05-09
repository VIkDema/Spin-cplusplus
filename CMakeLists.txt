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
    src/version/version.hpp.in
    src/fatal/fatal.hpp
    src/fatal/fatal.cpp
    src/utils/format/pretty_print_viewer.hpp
    src/utils/format/pretty_print_viewer.cpp
    src/utils/format/preprocessed_file_viewer.hpp
    src/utils/format/preprocessed_file_viewer.cpp
    src/utils/seed/seed.hpp
    src/utils/seed/seed.cpp
    src/utils/verbose/verbose.hpp
    src/utils/verbose/verbose.cpp
    src/utils/memory.hpp
    src/utils/memory.cpp
    src/lexer/lexer.hpp
    src/lexer/lexer.cpp
    src/lexer/names.cpp
    src/lexer/names.hpp
    src/lexer/file_stream.cpp
    src/lexer/file_stream.hpp
    src/lexer/inline_processor.cpp
    src/lexer/inline_processor.hpp
    src/lexer/line_number.cpp
    src/lexer/line_number.hpp
    src/lexer/yylex.cpp
    src/lexer/yylex.hpp
    src/lexer/helpers.hpp
    src/lexer/helpers.cpp
    src/lexer/deferred.hpp
    src/lexer/deferred.cpp
    src/lexer/scope.hpp
    src/lexer/scope.cpp
    src/main/arguments_parser.hpp
    src/main/arguments_parser.cpp
    src/main/help.cpp
    src/main/help.hpp
    src/main/launch_settings.hpp
    src/main/launch_settings.cpp
    src/main/main_processor.hpp
    src/main/main_processor.cpp
    src/main/pre_proc_settings.hpp
    src/main/pre_proc_settings.cpp
    src/main/pan_processor.hpp
    src/main/pan_processor.cpp
    src/models/access.hpp
    src/models/symbol.hpp
    src/models/symbol.cpp
    src/models/fms.hpp
    src/models/lextok.hpp
    src/models/lextok.cpp
    src/models/mtypes.hpp
    src/models/slicer.hpp
    src/models/queue.hpp
    src/models/itype.hpp
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

cmake_minimum_required(VERSION 3.18.4)
include(GoogleTest)

project(tu)

# Options
if ( ENABLE_COVERAGE )
	set(CMAKE_CXX_FLAGS "-g -O0 -Wall -W -Wshadow -Wunused-variable -Wunused-parameter -Wunused-function -Wunused -Wno-system-headers -Wno-deprecated -Woverloaded-virtual -Wwrite-strings -Wno-pointer-to-int-cast -fprofile-instr-generate -fcoverage-mapping")
endif ()

set(SRCS
    tu_all.cpp
    tu_cgmath.cpp
    )
	
add_executable(tu_cgmath ${SRCS})

gtest_discover_tests(tu_cgmath)

target_link_libraries(tu_cgmath gtest pthread gcov)

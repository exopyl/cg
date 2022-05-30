cmake_minimum_required(VERSION 3.18.4)
include(GoogleTest)

project(tu_cgmath)

set(SRCS
    tu_all.cpp
    tu_cgmath.cpp
    )
    
add_executable(tu_cgmath ${SRCS})

gtest_discover_tests(tu_cgmath)

target_link_libraries(tu_cgmath cgmath gtest pthread)

install(TARGETS tu_cgmath DESTINATION .)

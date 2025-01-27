cmake_minimum_required(VERSION 3.18.4)
include(GoogleTest)

project(tu_cgmath)

# Options
if ( ENABLE_COVERAGE )
	set(CMAKE_CXX_FLAGS "-g -O0 -Wall -W -Wshadow -Wunused-variable -Wunused-parameter -Wunused-function -Wunused -Wno-system-headers -Wno-deprecated -Woverloaded-virtual -Wwrite-strings -Wno-pointer-to-int-cast -fprofile-instr-generate -fcoverage-mapping")
endif ()

set(SRCS
    tu_all.cpp
    tu_cgmath_aabox.cpp
    tu_cgmath_algebra_vector3.cpp
    tu_cgmath_triangle.cpp
    )
	
add_executable(${PROJECT_NAME} ${SRCS})

target_link_libraries(${PROJECT_NAME} cgmath)

gtest_discover_tests(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} gtest pthread gcov)

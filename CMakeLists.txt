cmake_minimum_required (VERSION 3.18.4)

project (cg)

set(CMAKE_CXX_STANDARD 17)

# Options
set(ENABLE_COVERAGE Off CACHE BOOL "Enable coverage.")

add_subdirectory(src)
add_subdirectory(test)

if ( ENABLE_COVERAGE )
	add_custom_target (coverage
		COMMAND ctest -T Coverage
		COMMAND gcovr -x coverage.xml -g Testing/CoverageInfo/
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif ()

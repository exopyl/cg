cmake_minimum_required (VERSION 3.18.4)

project (cg)

set(CMAKE_CXX_STANDARD 17)

# Options
set(ENABLE_COVERAGE Off CACHE BOOL "Enable coverage.")
set(ENABLE_SINAIA Off CACHE BOOL "Enable sinaia.")

add_subdirectory(src/cgmath)
add_subdirectory(src/cgimg)
add_subdirectory(src/cgmesh)
if ( ENABLE_SINAIA )
	add_subdirectory(src/cgre)
endif ()

add_subdirectory(test)


if ( ENABLE_SINAIA )
	add_subdirectory(sinaia)
endif ()

if ( ENABLE_COVERAGE )
	add_custom_target (coverage
		COMMAND ctest -T Coverage
		COMMAND gcovr -x coverage.xml -g Testing/CoverageInfo/
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif ()

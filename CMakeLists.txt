cmake_minimum_required (VERSION 3.18.4)

project (cg)

set(CMAKE_CXX_STANDARD 17)

# Options
set(ENABLE_COVERAGE Off CACHE BOOL "Enable coverage.")
set(ENABLE_SINAIA Off CACHE BOOL "Enable sinaia.")
set(ENABLE_CGNET_DEMO Off CACHE BOOL "Build the cgnet_demo executable.")
set(ENABLE_OPENNURBS On CACHE BOOL "Enable openNURBS (.3dm) import.")
set(EXTERN_OPENNURBS "${CMAKE_SOURCE_DIR}/extern/opennurbs-v8.27.26019.16021" CACHE PATH "Path to openNURBS directory.")

if (ENABLE_OPENNURBS)
    if (EXISTS "${EXTERN_OPENNURBS}/include")
        include_directories("${EXTERN_OPENNURBS}/include")
        # Some opennurbs headers expect android_uuid/uuid.h to be findable
        # either via "android_uuid/uuid.h" or just "uuid.h" depending on config.
        include_directories("${EXTERN_OPENNURBS}/include/android_uuid")
    endif()
endif()

if ( ENABLE_COVERAGE )
	set(CMAKE_C_FLAGS "-g -O0 -fprofile-instr-generate -fcoverage-mapping")
	set(CMAKE_CXX_FLAGS "-g -O0 -Wall -W -Wshadow -Wunused-variable -Wunused-parameter -Wunused-function -Wunused -Wno-system-headers -Wno-deprecated -Woverloaded-virtual -Wwrite-strings -Wno-pointer-to-int-cast -fprofile-instr-generate -fcoverage-mapping")
endif ()

add_subdirectory(src/cgmath)
add_subdirectory(src/cgimg)
add_subdirectory(src/cgmesh)
if ( WIN32 )
	add_subdirectory(src/cgnet)
endif ()
if ( ENABLE_SINAIA )
	add_subdirectory(src/cgre)
endif ()

add_subdirectory(test)


if ( ENABLE_SINAIA )
	add_subdirectory(sinaia)
endif ()

if ( ENABLE_CGNET_DEMO AND WIN32 )
	add_subdirectory(cgnet_demo)
endif ()

if ( ENABLE_COVERAGE )
	add_custom_target (coverage
		COMMAND ctest -T Coverage
		COMMAND gcovr -x coverage.xml -g Testing/CoverageInfo/
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif ()

cmake_minimum_required (VERSION 3.18.4)

project (cg)

set(CMAKE_CXX_STANDARD 17)

# Options
set(ENABLE_COVERAGE Off CACHE BOOL "Enable coverage.")
set(ENABLE_SINAIA Off CACHE BOOL "Enable sinaia.")
set(ENABLE_CGNET_DEMO Off CACHE BOOL "Build the cgnet_demo executable.")
set(ENABLE_CGONNX_DEMO Off CACHE BOOL "Build the ONNX Runtime depth demo.")
set(ENABLE_ONNX Off CACHE BOOL "Enable the ONNX depth source in cgmesh (links ONNX Runtime).")
set(EXTERN_ONNXRUNTIME "" CACHE PATH "Path to ONNX Runtime prebuilt dir.")
if (NOT EXTERN_ONNXRUNTIME AND DEFINED ENV{EXTERN_ONNXRUNTIME})
    set(EXTERN_ONNXRUNTIME "$ENV{EXTERN_ONNXRUNTIME}")
endif()
# Le demo ONNX s'appuie sur la couche ONNX de cgmesh : l'activer l'implique.
if (ENABLE_CGONNX_DEMO)
    set(ENABLE_ONNX On)
endif()

set(ENABLE_POISSON Off CACHE BOOL "Enable Poisson surface reconstruction in cgmesh (vendored PoissonRecon).")
set(POISSONRECON_DIR "${CMAKE_SOURCE_DIR}/extern/PoissonRecon-18.76" CACHE PATH "Path to the vendored PoissonRecon source tree.")
set(ENABLE_SULINA Off CACHE BOOL "Compile sulina (Qt 6 + Vulkan, via cgre2).")
set(ENABLE_OPENNURBS On CACHE BOOL "Enable openNURBS (.3dm) import.")
set(EXTERN_OPENNURBS "" CACHE PATH "Path to openNURBS directory.")

if (ENABLE_OPENNURBS)
    if (NOT EXTERN_OPENNURBS AND DEFINED ENV{EXTERN_OPENNURBS})
        set(EXTERN_OPENNURBS "$ENV{EXTERN_OPENNURBS}")
    endif()

    if (EXISTS "${EXTERN_OPENNURBS}/include")
        include_directories("${EXTERN_OPENNURBS}/include")
        # Some opennurbs headers expect android_uuid/uuid.h to be findable
        # either via "android_uuid/uuid.h" or just "uuid.h" depending on config.
        include_directories("${EXTERN_OPENNURBS}/include/android_uuid")
    endif()
endif()

set(ENABLE_OCCT Off CACHE BOOL "Enable OCCT support.")
set(EXTERN_OCCT "" CACHE PATH "Path to OCCT directory.")

if (ENABLE_OCCT)
    if (NOT EXTERN_OCCT AND DEFINED ENV{EXTERN_OCCT})
        set(EXTERN_OCCT "$ENV{EXTERN_OCCT}")
    endif()

    if (WIN32)
        include_directories("${EXTERN_OCCT}/include")
    else()
        include_directories("${EXTERN_OCCT}/include/opencascade")
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

enable_testing()
add_subdirectory(test)


if ( ENABLE_SINAIA )
	add_subdirectory(sinaia)
endif ()

if ( ENABLE_CGNET_DEMO AND WIN32 )
	add_subdirectory(cgnet_demo)
endif ()

if ( ENABLE_CGONNX_DEMO )
	add_subdirectory(reconstruction-cli)
endif ()

# sulina (Qt 6 + Vulkan): pulls in the cgre2 render lib and the Qt app.
# Needs Qt 6.11 (CMAKE_PREFIX_PATH) and the Vulkan SDK (VULKAN_SDK).
if ( ENABLE_SULINA )
	add_subdirectory(src/cgre2)
	add_subdirectory(sulina)
endif ()

if ( ENABLE_COVERAGE )
	add_custom_target (coverage
		COMMAND ctest -T Coverage
		COMMAND gcovr -x coverage.xml -g Testing/CoverageInfo/
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif ()

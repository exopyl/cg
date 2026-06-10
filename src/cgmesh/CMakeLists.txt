cmake_minimum_required(VERSION 3.18.4)

project(libcgmesh)

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)
# Paths relative to cgmesh's own location so this works whether cgmesh is
# built from the repo-root CMakeLists or as a subproject of vecna/.
file(GLOB GLUTESS_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../extern/glutess/*.c)
file(GLOB GLUTESS_HEADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../extern/glutess/*.h)

if (UNIX)
	add_compile_options(-fpermissive)
endif (UNIX)

add_library(cgmesh STATIC ${HEADER_FILES} ${SRC_FILES} ${GLUTESS_HEADER_FILES} ${GLUTESS_SRC_FILES})
source_group("glutess" FILES ${GLUTESS_HEADER_FILES} ${GLUTESS_SRC_FILES})
target_include_directories(cgmesh PUBLIC ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../../extern)
target_link_libraries(cgmesh cgimg)

# std::thread (thickness parallelisation) needs pthread on Linux; Threads::Threads
# is a no-op on MSVC. Plain signature (matching the cgimg link above) propagates
# transitively, so consumers like the test exe link it too.
find_package(Threads REQUIRED)
target_link_libraries(cgmesh Threads::Threads)

if (ENABLE_OPENNURBS)
    if (NOT EXTERN_OPENNURBS AND DEFINED ENV{EXTERN_OPENNURBS})
        set(EXTERN_OPENNURBS "$ENV{EXTERN_OPENNURBS}")
    endif()

    if (NOT EXTERN_OPENNURBS)
        message(FATAL_ERROR "ENABLE_OPENNURBS is ON but EXTERN_OPENNURBS is not defined.")
    endif()
    
    if (NOT EXISTS "${EXTERN_OPENNURBS}/include")
        message(FATAL_ERROR "ENABLE_OPENNURBS is ON but EXTERN_OPENNURBS/include does not exist at ${EXTERN_OPENNURBS}/include")
    endif()

    target_compile_definitions(cgmesh PUBLIC CG_HAS_OPENNURBS)
    target_compile_definitions(cgmesh PRIVATE OPENNURBS_IMPORTS)
    target_include_directories(cgmesh PUBLIC ${EXTERN_OPENNURBS}/include)

    if (WIN32)
        target_link_libraries(cgmesh ${EXTERN_OPENNURBS}/lib/vc14x_x64/OpenNURBS.lib)
    elseif (UNIX)
        target_link_libraries(cgmesh ${EXTERN_OPENNURBS}/lib/linux_x64/libOpenNURBS.so)
    endif()
endif()

if (ENABLE_OCCT)
    target_compile_definitions(cgmesh PUBLIC CG_HAS_OCCT)

    if (WIN32)
        # Pre-built OCCT from cg_binaries (or a local extern dir) — there
        # is no apt equivalent on Windows. EXTERN_OCCT must point at a
        # directory containing include/ and lib/.
        if (NOT EXTERN_OCCT AND DEFINED ENV{EXTERN_OCCT})
            set(EXTERN_OCCT "$ENV{EXTERN_OCCT}")
        endif()
        if (NOT EXTERN_OCCT)
            message(FATAL_ERROR "ENABLE_OCCT is ON (Windows) but EXTERN_OCCT is not defined.")
        endif()
        if (NOT EXISTS "${EXTERN_OCCT}/include")
            message(FATAL_ERROR "EXTERN_OCCT/include does not exist at ${EXTERN_OCCT}/include")
        endif()

        target_include_directories(cgmesh PUBLIC ${EXTERN_OCCT}/include)

        # OCCT 8.0 consolidated the legacy TKSTEP/TKSTEPBase/TKSTEP209/
        # TKSTEPAttr libs into TKDESTEP (and TKIGES into TKDEIGES). The
        # cg_binaries Windows archive ships 8.0, so this list matches.
        set(_OCCT_LIBS
            TKernel
            TKMath
            TKG3d
            TKGeomBase
            TKGeomAlgo
            TKBRep
            TKTopAlgo
            TKBO
            TKMesh
            TKXSBase
            TKDE
            TKDESTEP
            TKDEIGES
        )
        foreach(_lib ${_OCCT_LIBS})
            target_link_libraries(cgmesh ${EXTERN_OCCT}/lib/${_lib}.lib)
        endforeach()
    elseif (UNIX)
        # apt-installed OCCT (libocct-foundation-dev, libocct-modeling-
        # data-dev, libocct-modeling-algorithms-dev, libocct-data-exchange-
        # dev). The Debian/Ubuntu packages ship a CMake config at
        # /usr/lib/x86_64-linux-gnu/cmake/opencascade/ that find_package
        # discovers automatically — no EXTERN_OCCT to manage.
        find_package(OpenCASCADE REQUIRED)
        target_include_directories(cgmesh PUBLIC ${OpenCASCADE_INCLUDE_DIR})

        # We deliberately AVOID linking ${OpenCASCADE_LIBRARIES} wholesale.
        # The apt-shipped CMake config propagates references to the Draw
        # test framework (TKDraw, TKDCAF, TKViewerTest, …) and to
        # libfontconfig via the imported targets' INTERFACE_LINK_LIBRARIES.
        # Those libs live in apt packages we don't otherwise need
        # (libocct-draw, libocct-visualization-dev, libfontconfig-dev) and
        # their .so symlinks aren't always shipped, so the link fails.
        # Picking the libs explicitly keeps the apt surface minimal and
        # mirrors how the Windows branch does it.
        #
        # OCCT 7.8 consolidated TKSTEP/TKSTEPBase/TKSTEP209/TKSTEPAttr
        # into TKDESTEP, and TKIGES into TKDEIGES. Older Ubuntus still
        # ship 7.6/7.7 with the legacy lib names.
        set(_OCCT_LIBS_LINUX
            TKernel TKMath
            TKG3d TKGeomBase TKGeomAlgo
            TKBRep TKTopAlgo TKBO TKShHealing
            TKMesh
            TKXSBase
        )
        if (DEFINED OpenCASCADE_VERSION AND OpenCASCADE_VERSION VERSION_GREATER_EQUAL 7.8)
            list(APPEND _OCCT_LIBS_LINUX TKDE TKDESTEP TKDEIGES)
        else()
            list(APPEND _OCCT_LIBS_LINUX TKSTEP TKSTEPBase TKSTEP209 TKSTEPAttr TKIGES)
        endif()
        # Link by absolute path (via OpenCASCADE_LIBRARY_DIR exposed by
        # find_package) — bypasses the imported targets, so their
        # over-eager INTERFACE_LINK_LIBRARIES are not pulled in.
        foreach(_lib ${_OCCT_LIBS_LINUX})
            target_link_libraries(cgmesh ${OpenCASCADE_LIBRARY_DIR}/lib${_lib}.so)
        endforeach()
    endif()
endif()

install(TARGETS cgmesh DESTINATION .)

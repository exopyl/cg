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

if (ENABLE_ONNX)
    if (NOT EXTERN_ONNXRUNTIME AND DEFINED ENV{EXTERN_ONNXRUNTIME})
        set(EXTERN_ONNXRUNTIME "$ENV{EXTERN_ONNXRUNTIME}")
    endif()
    if (NOT EXTERN_ONNXRUNTIME)
        message(FATAL_ERROR "ENABLE_ONNX is ON but EXTERN_ONNXRUNTIME is not defined.")
    endif()
    if (NOT EXISTS "${EXTERN_ONNXRUNTIME}/include")
        message(FATAL_ERROR "EXTERN_ONNXRUNTIME/include does not exist at ${EXTERN_ONNXRUNTIME}/include")
    endif()

    # CG_HAS_ONNX (PUBLIC) : la classe recon::OnnxDepthSource n'existe que sous ce
    # define ; PUBLIC pour que les consommateurs (reconstruction-cli) la voient aussi.
    target_compile_definitions(cgmesh PUBLIC CG_HAS_ONNX)
    target_include_directories(cgmesh PUBLIC ${EXTERN_ONNXRUNTIME}/include)
    if (WIN32)
        target_link_libraries(cgmesh ${EXTERN_ONNXRUNTIME}/lib/onnxruntime.lib)
    elseif (UNIX)
        target_link_libraries(cgmesh ${EXTERN_ONNXRUNTIME}/lib/libonnxruntime.so)
    endif()
endif()

if (ENABLE_POISSON)
    if (NOT EXISTS "${POISSONRECON_DIR}/Src/Reconstructors.h")
        message(FATAL_ERROR "ENABLE_POISSON: Reconstructors.h introuvable dans ${POISSONRECON_DIR}/Src")
    endif()
    # CG_HAS_POISSON (PUBLIC) : recon::PoissonReconstructor n'existe que sous ce define.
    # Les en-têtes PoissonRecon restent PRIVATE (cachés dans recon_surface_poisson.cpp).
    target_compile_definitions(cgmesh PUBLIC CG_HAS_POISSON)
    target_include_directories(cgmesh PRIVATE "${POISSONRECON_DIR}/Src")
    if (MSVC)
        # _HAS_STD_BYTE=0 : PoissonRecon fait `using namespace std` + inclut des en-têtes
        # Windows -> std::byte vs byte du SDK = "symbole ambigu". On désactive std::byte.
        target_compile_definitions(cgmesh PRIVATE _USE_MATH_DEFINES NOMINMAX _HAS_STD_BYTE=0)
        target_compile_options(cgmesh PRIVATE /bigobj)   # templates FEMTree volumineux
    endif()
endif()

if (ENABLE_ZLIB)
    # CG_HAS_ZLIB (PUBLIC) : gate nbt.{h,cpp}; PUBLIC so consumers that pull in
    # cgmesh.h (which includes nbt.h) see the same guard.
    target_compile_definitions(cgmesh PUBLIC CG_HAS_ZLIB)

    if (WIN32)
        # Windows has no system zlib, and OpenNURBS/OCCT ship as DLLs whose own
        # zlib is private (PE has no cross-module symbol interposition), so we
        # vendor-compile extern/zlib-1.3.2 straight into cgmesh (glutess pattern).
        if (NOT EXISTS "${ZLIB_DIR}/zlib.h")
            message(FATAL_ERROR "ENABLE_ZLIB is ON but zlib.h is not found at ${ZLIB_DIR}/zlib.h")
        endif()
        file(GLOB ZLIB_SRC_FILES ${ZLIB_DIR}/*.c)
        target_sources(cgmesh PRIVATE ${ZLIB_SRC_FILES})
        source_group("zlib" FILES ${ZLIB_SRC_FILES})
        # PUBLIC : nbt.h does #include "zlib.h", so consumers of cgmesh.h need the
        # zlib headers on their include path too.
        target_include_directories(cgmesh PUBLIC ${ZLIB_DIR})
        if (MSVC)
            # zlib uses fopen/strerror/etc.; silence the MSVC CRT deprecation noise.
            set_source_files_properties(${ZLIB_SRC_FILES} PROPERTIES
                COMPILE_DEFINITIONS "_CRT_SECURE_NO_WARNINGS;_CRT_NONSTDC_NO_DEPRECATE")
        endif()
    else()
        # Linux/Unix: link the SYSTEM zlib -- the SAME copy OpenNURBS and OCCT
        # already use. Vendoring a SECOND zlib put our inflate/deflate into the
        # ELF global symbol namespace: unprefixed it interposed over OpenNURBS'
        # bundled zlib (SEGFAULT loading .3dm); prefixing ours (Z_PREFIX) then left
        # OpenNURBS' decompression unable to resolve the zlib it needs (.3dm load
        # returned false). A single, shared system zlib removes the conflict
        # entirely and matches how this repo already consumes OCCT on Linux.
        # Requires the zlib1g-dev package on the build host (see build.yml).
        find_package(ZLIB REQUIRED)
        target_include_directories(cgmesh PUBLIC ${ZLIB_INCLUDE_DIRS})
        target_link_libraries(cgmesh ${ZLIB_LIBRARIES})
    endif()
endif()

install(TARGETS cgmesh DESTINATION .)

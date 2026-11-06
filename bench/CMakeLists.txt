cmake_minimum_required(VERSION 3.18.4)

project(cg_bench)

# Bancs de performance (Google Benchmark). Ajoute par le CMakeLists racine sous
# ENABLE_BENCHMARKS seulement. Reproduction et mesures : baseline_performance.md.
#
# Un seul executable, comme TU : main vient de benchmark_main, l'operator new
# global est remplace par allocation_counter.cpp (compteurs allocs et
# alloc_bytes). Ce remplacement vaut pour tout le programme : il ne doit pas
# rejoindre un autre executable.
file(GLOB BENCH_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB BENCH_HEADERS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/*.h)

add_executable(cg_bench ${BENCH_HEADERS} ${BENCH_SOURCES})
target_link_libraries(cg_bench PRIVATE cgarchi cgmesh benchmark::benchmark_main)
target_compile_features(cg_bench PRIVATE cxx_std_17)
target_compile_options(cg_bench PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)

# Bibliotheques partagees liees par cgmesh : deposees a cote de l'executable,
# sans quoi il ne demarre pas sous Windows. Memes sources que pour TU
# (test/CMakeLists.txt).
if (WIN32 AND ENABLE_OPENNURBS)
    add_custom_command(TARGET cg_bench POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_OPENNURBS}/lib/vc14x_x64/OpenNURBS.dll"
            "$<TARGET_FILE_DIR:cg_bench>"
        COMMENT "Copying OpenNURBS.dll to $<TARGET_FILE_DIR:cg_bench>")
endif ()
if (WIN32 AND ENABLE_LIB3MF)
    add_custom_command(TARGET cg_bench POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_LIB3MF}/bin/lib3mf.dll"
            "$<TARGET_FILE_DIR:cg_bench>"
        COMMENT "Copying lib3mf.dll to $<TARGET_FILE_DIR:cg_bench>")
endif ()
if (WIN32 AND ENABLE_OCCT)
    add_custom_command(TARGET cg_bench POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E ${CG_COPY_DIRECTORY}
            "${EXTERN_OCCT}/bin"
            "$<TARGET_FILE_DIR:cg_bench>"
        COMMENT "Copying OCCT DLLs to $<TARGET_FILE_DIR:cg_bench>")
endif ()
if (WIN32 AND ENABLE_ONNX)
    add_custom_command(TARGET cg_bench POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_ONNXRUNTIME}/lib/onnxruntime.dll"
            "$<TARGET_FILE_DIR:cg_bench>"
        COMMENT "Copying onnxruntime.dll to $<TARGET_FILE_DIR:cg_bench>")
endif ()

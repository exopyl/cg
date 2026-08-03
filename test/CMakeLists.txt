cmake_minimum_required(VERSION 3.18.4)
include(GoogleTest)

project(TU)

# Options
if ( ENABLE_COVERAGE )
	set(CMAKE_CXX_FLAGS "-g -O0 -Wall -W -Wshadow -Wunused-variable -Wunused-parameter -Wunused-function -Wunused -Wno-system-headers -Wno-deprecated -Woverloaded-virtual -Wwrite-strings -Wno-pointer-to-int-cast -fprofile-instr-generate -fcoverage-mapping")
endif ()

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)

add_executable(${PROJECT_NAME} ${HEADER_FILES} ${SRC_FILES})

# ---------------------------------------------------------------------------
# Repertoire de travail des tests
# ---------------------------------------------------------------------------
# Les tests ecrivent leurs sorties via des noms relatifs NUS ("shapes_cube.obj",
# "img_quant_wu.ppm", ... : 52 noms, plus ~155 SVG produits par les L-systemes) et
# lisent leurs entrees via "./test/data/..." (63 sites). Entrees et sorties
# partagent donc le meme repertoire courant, et aucun code de test ne le choisit :
# il est herite du lanceur. D'ou deux symptomes :
#
#   - lance a la main ou par coverage.sh depuis la racine, TU deverse ses 200+
#     fichiers de sortie DANS la racine du depot ;
#   - ctest, lui, tournait dans CMAKE_CURRENT_BINARY_DIR (build/<preset>/test)
#     alors que test/data etait copie dans $<TARGET_FILE_DIR> -- soit un niveau
#     PLUS BAS en multi-config (build/<preset>/test/Debug). Les tests lisant
#     test/data echouaient donc sous ctest sur Windows.
#
# Correctif : un sous-repertoire DEDIE, impose a ctest comme au debogueur, sous
# lequel test/data est copie. Les sorties y sont confinees (et sous build*/, donc
# ignore par git) ; le chargement des DLL n'est pas affecte, Windows les cherchant
# dans le dossier de l'exe et non dans le repertoire courant.
set(TU_RUN_DIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>/test-run")

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${TU_RUN_DIR}"
)

# Cree sans condition : c'est le repertoire de travail annonce a ctest, il doit
# exister meme si le depot n'embarque pas de test/data.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${TU_RUN_DIR}"
    COMMENT "Preparing test working directory ${TU_RUN_DIR}"
)

if(EXISTS "${CMAKE_SOURCE_DIR}/test/data")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/test/data"
            "${TU_RUN_DIR}/test/data"
        COMMENT "Copying test/data to ${TU_RUN_DIR}/test/data"
    )
endif()

if (UNIX AND ENABLE_OPENNURBS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_OPENNURBS}/lib/linux_x64/libOpenNURBS.so"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying libOpenNURBS.so to $<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()

if (WIN32 AND ENABLE_OPENNURBS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_OPENNURBS}/lib/vc14x_x64/OpenNURBS.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying OpenNURBS.dll to $<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()

# OCCT ships ~48 interdependent DLLs; copy the whole bin/ folder next to
# the test exe so STEP-loading tests can resolve TKernel.dll & friends
# without polluting PATH. copy_directory is idempotent; cost is one-time.
if (WIN32 AND ENABLE_OCCT)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${EXTERN_OCCT}/bin"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying OCCT DLLs to $<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()

# cgmesh importe onnxruntime.dll sous ENABLE_ONNX -> l'exe de test en a besoin.
if (WIN32 AND ENABLE_ONNX)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_ONNXRUNTIME}/lib/onnxruntime.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying onnxruntime.dll to $<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()

if (WIN32 AND EXTERN_GOOGLETEST)
    add_subdirectory(${EXTERN_GOOGLETEST} gtest)
    target_link_libraries(${PROJECT_NAME} GTest::gtest_main)
    target_link_libraries(${PROJECT_NAME} GTest::gmock_main)
elseif (NOT WIN32)
    find_package(GTest REQUIRED)
    target_link_libraries(${PROJECT_NAME} GTest::gtest_main)
endif (WIN32 AND EXTERN_GOOGLETEST)

# WORKING_DIRECTORY explicite : sans lui, ctest utilise CMAKE_CURRENT_BINARY_DIR,
# qui n'est pas la ou test/data est copie en multi-config (cf. TU_RUN_DIR ci-dessus).
gtest_discover_tests(${PROJECT_NAME}
    WORKING_DIRECTORY "${TU_RUN_DIR}"
)

target_link_libraries(${PROJECT_NAME} cgmath cgimg cgmesh)
if ( WIN32 )
    target_link_libraries(${PROJECT_NAME} cgnet)
endif ()
if ( ENABLE_COVERAGE )
    if ( UNIX )
        target_link_libraries(${PROJECT_NAME} pthread gcov)
    endif ()
endif ()

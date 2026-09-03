cmake_minimum_required(VERSION 3.18.4)
include(GoogleTest)

project(TU)

# Options
if ( ENABLE_COVERAGE )
	set(CMAKE_CXX_FLAGS "-g -O0 -Wall -W -Wshadow -Wunused-variable -Wunused-parameter -Wunused-function -Wunused -Wno-system-headers -Wno-deprecated -Woverloaded-virtual -Wwrite-strings -Wno-pointer-to-int-cast -fprofile-instr-generate -fcoverage-mapping")
endif ()

file(GLOB SRC_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/*.h)

add_executable(${PROJECT_NAME} ${HEADER_FILES} ${SRC_FILES})

# ---------------------------------------------------------------------------
# Commande de copie de repertoire
# ---------------------------------------------------------------------------
# La copie POST_BUILD passe par CG_COPY_DIRECTORY, defini par le CMakeLists
# racine : copy_directory reecrit systematiquement et bute sur un verrou.

# ---------------------------------------------------------------------------
# Repertoire de travail des tests
# ---------------------------------------------------------------------------
# Les tests ecrivent leurs sorties via des noms relatifs NUS ("shapes_cube.obj",
# "img_quant_wu.ppm", ... : 52 noms, plus ~155 SVG produits par les L-systemes) et
# lisent leurs entrees via "./test/data/..." (63 sites). Entrees et sorties
# partagent donc le meme repertoire courant, et aucun code de test ne le choisit :
# il est herite du lanceur. D'ou un repertoire DEDIE, sous lequel test/data est
# copie et dans lequel les sorties sont confinees -- sans lui elles se deversent
# dans le repertoire courant du lanceur, soit la racine du depot quand TU est
# lance a la main ou par coverage.sh. Il est sous build*/, donc ignore par git.
#
# Ce repertoire est impose A LA FOIS a ctest et au debogueur : les deux voies
# d'execution doivent viser le meme endroit, faute de quoi les tests lisant
# test/data passent dans l'une et echouent dans l'autre.
#
# Il est INDEPENDANT DE LA CONFIGURATION, donc exempt d'expression de generateur :
# Visual Studio en mode "Ouvrir un dossier" lit VS_DEBUGGER_WORKING_DIRECTORY via
# le CMake File API, qui expose la valeur brute sans l'evaluer. Une expression de
# generateur y arriverait litteralement et le debogage echouerait sur un
# repertoire de travail invalide.
#
# Consequence : Debug et Release partagent ce repertoire. La copie de test/data y
# est idempotente ; en revanche les sorties de test portent les memes noms d'une
# configuration a l'autre, donc ne pas executer les deux suites simultanement.
#
# Le chargement des DLL n'est pas concerne : Windows cherche les DLL dans le
# dossier de l'exe -- lui par configuration -- et non dans le repertoire courant.
set(TU_RUN_DIR "${CMAKE_CURRENT_BINARY_DIR}/test-run")

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
        COMMAND ${CMAKE_COMMAND} -E ${CG_COPY_DIRECTORY}
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

# lib3mf est liee en IMPLICITE : le .dll doit etre trouvable au lancement, sinon
# TU.exe ne demarre meme pas (l'erreur precede tout code de test).
if (WIN32 AND ENABLE_LIB3MF)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_LIB3MF}/bin/lib3mf.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying lib3mf.dll to $<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()

# Linux : on copie "lib3mf.so.2" et NON "lib3mf.so".
#
# Le SONAME de la bibliotheque est `lib3mf.so.2` (lu dans l'ELF) : c'est ce nom
# que ld.so cherche a l'execution. `lib3mf.so` n'est que le lien de
# developpement utilise a l'edition de liens ; le copier seul ne servirait a
# rien. copy_if_different suit le lien symbolique et depose un fichier reel sous
# le bon nom.
#
# Ce qui resout REELLEMENT la dependance pendant ctest, c'est le RPATH de build :
# cgmesh liant ${EXTERN_LIB3MF}/lib/lib3mf.so par chemin absolu, CMake ajoute ce
# repertoire au RPATH de TU. Cette copie double la garantie et aligne le bloc sur
# celui d'openNURBS ci-dessus ; elle sert des que le binaire est execute hors de
# l'arbre de build.
if (UNIX AND ENABLE_LIB3MF)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_LIB3MF}/lib/lib3mf.so.2"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying lib3mf.so.2 to $<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()

# OCCT ships ~48 interdependent DLLs; copy the whole bin/ folder next to
# the test exe so STEP-loading tests can resolve TKernel.dll & friends
# without polluting PATH. The copy is skipped file by file once the
# destination matches, so a settled tree writes nothing here.
if (WIN32 AND ENABLE_OCCT)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E ${CG_COPY_DIRECTORY}
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
# le parent de TU_RUN_DIR, ou test/data n'est pas copie (cf. TU_RUN_DIR ci-dessus).
#
# DISCOVERY_TIMEOUT : la decouverte lance TU.exe juste apres que les commandes
# POST_BUILD ci-dessus ont depose ~50 DLL (OCCT, OpenNURBS, lib3mf) a cote de
# l'executable. A froid, leur chargement depasse les 5 s du defaut et la
# decouverte echoue sur un timeout, sans sortie. A chaud le listage prend ~2 s :
# la marge du defaut est trop mince, celle-ci ne coute rien quand tout va bien.
gtest_discover_tests(${PROJECT_NAME}
    WORKING_DIRECTORY "${TU_RUN_DIR}"
    DISCOVERY_TIMEOUT 60
)

target_link_libraries(${PROJECT_NAME} cgmath cgimg cgmesh cggraph cggraph_nodes cggraph_ui)
if ( WIN32 )
    target_link_libraries(${PROJECT_NAME} cgnet)
endif ()
if ( ENABLE_COVERAGE )
    if ( UNIX )
        target_link_libraries(${PROJECT_NAME} pthread gcov)
    endif ()
endif ()

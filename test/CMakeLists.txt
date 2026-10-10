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

# zlib pour le SEUL test qui inclut nbt.h en direct.
#
# ${ZLIB_DIR} est PRIVATE sur la cible cgmesh : nbt.h inclut zlib.h, et l'exposer
# en PUBLIC mettait les en-tetes zlib sur le chemin d'inclusion des 43
# consommateurs du parapluie hors module. tu_cgmesh_nbt.cpp est le seul a en avoir
# reellement besoin, il le demande donc pour lui-meme.
if (ENABLE_ZLIB AND WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE ${ZLIB_DIR})
elseif (ENABLE_ZLIB)
    find_package(ZLIB REQUIRED)
    target_include_directories(${PROJECT_NAME} PRIVATE ${ZLIB_INCLUDE_DIRS})
endif ()

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

# cgre : la cible n'existe que sous ENABLE_CGRE (voir le CMakeLists racine), que
# ENABLE_SINAIA implique et que le preset ci-linux active. Le module se batit sur
# les deux plateformes : le chargeur WGL est isole sous WIN32, le reste est
# portable.
#
# Le fichier de test est ramasse par le file(GLOB) ci-dessus quelle que soit la
# plateforme : c'est CG_HAS_CGRE qui le vide sur les configurations sans cgre.
# Sans cette definition, il ne compile a rien.
if ( TARGET cgre )
    target_link_libraries(${PROJECT_NAME} cgre)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CG_HAS_CGRE)
endif ()

# ---------------------------------------------------------------------------
# tu_peles_core.cpp : les fonctions pures de peles_core
# ---------------------------------------------------------------------------
#
# ⚠ CONDITION DISTINCTE DE CELLE DE tu_supported_formats CI-DESSOUS, ET C'EST
# LE POINT : ce fichier n'inclut RIEN de sinaia, donc il n'a besoin ni de
# wxWidgets ni de ses DLL. Il tourne sous ENABLE_PELES seul -- c'est-a-dire dans
# une configuration qu'une CI Linux peut reunir, ce que ENABLE_SINAIA=ON ne
# permet pas.
#
# ⚠ MEME REMARQUE QUE CI-DESSOUS SUR LA CONDITION : elle porte sur l'OPTION et
# non sur `TARGET peles_core`, parce que ce repertoire est ajoute AVANT peles/
# par le CMakeLists racine. Lier la cible est legitime -- CMake resout les
# cibles a la GENERATION -- mais l'interroger ne l'est pas.
if ( ENABLE_PELES )
    target_link_libraries(${PROJECT_NAME} peles_core)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CG_HAS_PELES_CORE)
endif ()

# ---------------------------------------------------------------------------
# tu_supported_formats.cpp : le catalogue de formats, des deux cotes
# ---------------------------------------------------------------------------
#
# Il compare le catalogue DETYPE de peles (peles/core/file_catalog.h) a celui
# de sinaia (sinaia/SupportedFormats.h). Il exige donc les DEUX : peles_core
# pour le premier, et les en-tetes de wxWidgets pour que le second soit
# incluable. Hors de cette configuration, CG_PELES_SINAIA_FORMATS n'est pas
# defini et le fichier ne compile a rien -- meme modele que CG_HAS_CGRE
# ci-dessus, le file(GLOB) le ramassant quelle que soit la plateforme.
#
# ⚠ wxWidgets EST LIE, ET SES DEUX DLL SONT DEPOSEES A COTE DE TU.exe. Le test
# n'appelle pourtant aucune fonction de wxWidgets -- il lit la DONNEE du
# catalogue, dont les extensions sont des `const wxChar*`. MESURE : les en-tetes
# seuls ne suffisent pas, `#include <wx/string.h>` fait reference au destructeur
# de wxString (LNK2019 __imp_??1wxString@@QEAA@XZ) sans qu'aucune ligne du test
# ne nomme ce type. La dependance est donc reelle, et la taire ferait echouer
# l'edition de liens de la cible ENTIERE.
#
# ⚠ SANS LA COPIE DES DLL, AUCUN TEST NE DEMARRERAIT -- pas seulement celui-ci.
# C'est le meme mecanisme que les copies d'OpenNURBS, lib3mf et OCCT ci-dessus,
# et il est soumis a la meme condition qu'elles : il n'existe que dans la
# configuration qui le rend necessaire.
#
# find_package est refait ici : les variables posees par sinaia/CMakeLists.txt
# ont la portee de SON repertoire et ne sont pas visibles depuis celui-ci.
# ⚠ LA CONDITION PORTE SUR LES OPTIONS, PAS SUR `TARGET peles_core` : ce
# repertoire est ajoute AVANT peles/ par le CMakeLists racine, donc la cible
# n'existe pas encore ici. La lier quand meme est legitime -- CMake resout les
# cibles a la GENERATION, pas a la configuration -- mais l'interroger ne l'est
# pas : le test se compilerait a rien sans un mot.
if ( ENABLE_SINAIA AND ENABLE_PELES )
    set(wxWidgets_ROOT_DIR ${EXTERN_WXWIDGETS})
    find_package(wxWidgets REQUIRED COMPONENTS core base)
    # peles_core est deja lie par le bloc ENABLE_PELES ci-dessus : le nommer une
    # seconde fois ferait diverger les deux listes a la premiere evolution.
    target_link_libraries(${PROJECT_NAME} ${wxWidgets_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/sinaia
        ${wxWidgets_INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        CG_PELES_SINAIA_FORMATS
        ${wxWidgets_DEFINITIONS})

    if (WIN32)
        # Memes noms que sinaia/CMakeLists.txt, et seulement les deux composants
        # demandes ci-dessus.
        set(TU_WX_DLL_NAMES
            wxmsw332u$<IF:$<CONFIG:Debug>,d,>_core_vc14x_x64
            wxbase332u$<IF:$<CONFIG:Debug>,d,>_vc14x_x64)
        foreach(_tu_wx_dll ${TU_WX_DLL_NAMES})
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${wxWidgets_LIB_DIR}/${_tu_wx_dll}.dll"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
                COMMENT "Copying ${_tu_wx_dll}.dll to $<TARGET_FILE_DIR:${PROJECT_NAME}>")
        endforeach()
    endif ()
endif ()

if ( ENABLE_COVERAGE )
    if ( UNIX )
        target_link_libraries(${PROJECT_NAME} pthread gcov)
    endif ()
endif ()

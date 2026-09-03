cmake_minimum_required (VERSION 3.18.4)

project (cg)

set(CMAKE_CXX_STANDARD 17)

# REPLI DE TYPE DE BUILD. Sans lui, une configuration hors preset sur un
# generateur mono-config (Ninja, Makefiles) compile SANS AUCUN -O : cgmath et
# cgimg, dont les primitives sont appelees en boucle chaude par cgmesh, y
# perdent tout. Le motif est celui de vecna/CMakeLists.txt:18-19.
#
# Uniquement sur generateur mono-config : sur Visual Studio ou Xcode,
# CMAKE_BUILD_TYPE n'a pas de sens -- le type se choisit a la construction.
get_property(CG_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (NOT CG_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Type de build" FORCE)
    message(STATUS "CMAKE_BUILD_TYPE non precise : repli sur Release")
endif ()

# OPTIMISATION INTER-PROCEDURALE, derriere une option et par defaut OFF.
#
# cgmath et cgimg sont des bibliotheques STATIQUES dont les accesseurs par pixel
# et par composante vivent dans une autre unite de traduction que leurs
# appelants : sans LTO, aucun de ces acces n'est inlinable. C'est l'un des
# constats de debt_cgimg.md.
#
# OFF par defaut, et delibere : l'audit n'a mesure aucun gain -- il ne pouvait
# pas, aucun banc n'ayant ete execute -- et l'IPO allonge sensiblement l'edition
# de liens. L'option existe pour rendre la mesure possible sans modifier le
# depot, pas pour trancher a sa place.
option(CG_ENABLE_IPO "Optimisation inter-procedurale (LTO) en Release" OFF)
if (CG_ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT CG_IPO_OK OUTPUT CG_IPO_WHY)
    if (CG_IPO_OK)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        message(STATUS "IPO active en Release")
    else ()
        message(WARNING "IPO demandee mais indisponible : ${CG_IPO_WHY}")
    endif ()
endif ()

# copy_directory REECRIT SYSTEMATIQUEMENT : il ne compare ni dates ni contenu.
# Les blocs POST_BUILD qui deposent les ~50 DLL d'OCCT a cote d'un executable
# les reecrivaient donc a chaque build, et il suffisait qu'une seule soit
# verrouillee -- un binaire en cours d'execution tient ses DLL mappees -- pour
# que la commande sorte en "Permission denied" et fasse echouer le build, y
# compris quand rien n'etait a reconstruire.
#
# copy_directory_if_different ne recopie que ce qui differe : en regime etabli
# il n'ecrit plus rien, donc il ne peut plus buter sur un verrou. Il apparait en
# CMake 3.26 ; le plancher declare ci-dessus est 3.18.4, d'ou le repli.
#
# Defini ici pour que les sous-repertoires en heritent. vecna est un projet
# CMake distinct, non ajoute par cette racine : il porte sa propre copie.
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.26)
    set(CG_COPY_DIRECTORY copy_directory_if_different)
else ()
    set(CG_COPY_DIRECTORY copy_directory)
endif ()

# Sous Emscripten (cible maker, WebAssembly) : aucune dependance lourde n'est
# disponible ni souhaitee. On les force OFF ici defensivement — le preset
# maker-wasm les met deja a OFF, ceci protege une configuration manuelle.
# Cf. maker/ANALYSE.md sec.9.
if (EMSCRIPTEN)
    set(ENABLE_OPENNURBS Off)
    set(ENABLE_OCCT      Off)
    set(ENABLE_ONNX      Off)
    set(ENABLE_POISSON   Off)
    set(ENABLE_ZLIB      Off)
    set(ENABLE_SINAIA    Off)
    set(ENABLE_SULINA    Off)

    # -fexceptions sur TOUTE la cible WASM (cgmath, cgimg, cgmesh, maker).
    # Sans cela, un `throw` dans cgmath (ex. buildFoilRing sur des parametres
    # degeneres) ne peut PAS remonter jusqu'au catch(...) de cgmesh : l'unwinder
    # manque les tables dans les frames cgmath -> trap qui traverse vers JS au
    # lieu du repli placeholder. Doit preceder les add_subdirectory ci-dessous.
    add_compile_options(-fexceptions)
    add_link_options(-fexceptions)

    # -Wsuggest-destructor-override n'est ni dans -Wall ni dans -Wextra, et
    # -Winconsistent-missing-override (actif par defaut) exclut deliberement les
    # destructeurs. Un destructeur de sous-classe declare `virtual` au lieu
    # d'`override` passe donc en silence, et le jour ou la classe de base perd
    # son `virtual` la destruction polymorphe devient un comportement indefini
    # sans un mot du compilateur. Ce drapeau transforme ce silence en
    # diagnostic.
    #
    # Portee : TOUTE la configuration Emscripten, pas la seule cible maker. Le
    # diagnostic se declenche a la compilation de la classe, donc dans chaque
    # unite de traduction qui inclut l'en-tete ; le limiter a maker
    # n'inspecterait que ses deux .cpp.
    add_compile_options(-Wsuggest-destructor-override)
endif()

# Options
set(ENABLE_COVERAGE Off CACHE BOOL "Enable coverage.")
set(ENABLE_SINAIA Off CACHE BOOL "Enable sinaia.")
set(ENABLE_MAKER Off CACHE BOOL "Build the maker WebAssembly module (Emscripten only).")
set(ENABLE_CGNET_DEMO Off CACHE BOOL "Build the cgnet_demo executable.")
set(ENABLE_RECONSTRUCTION_CLI Off CACHE BOOL "Build the reconstruction-cli executable (image-based 3D reconstruction; links ONNX Runtime).")
set(ENABLE_ONNX Off CACHE BOOL "Enable the ONNX depth source in cgmesh (links ONNX Runtime).")
set(EXTERN_ONNXRUNTIME "" CACHE PATH "Path to ONNX Runtime prebuilt dir.")
if (NOT EXTERN_ONNXRUNTIME AND DEFINED ENV{EXTERN_ONNXRUNTIME})
    set(EXTERN_ONNXRUNTIME "$ENV{EXTERN_ONNXRUNTIME}")
endif()
# reconstruction-cli s'appuie sur la couche ONNX de cgmesh : l'activer l'implique.
if (ENABLE_RECONSTRUCTION_CLI)
    set(ENABLE_ONNX On)
endif()

set(ENABLE_POISSON Off CACHE BOOL "Enable Poisson surface reconstruction in cgmesh (vendored PoissonRecon).")
set(POISSONRECON_DIR "${CMAKE_SOURCE_DIR}/extern/PoissonRecon-18.76" CACHE PATH "Path to the vendored PoissonRecon source tree.")
set(ENABLE_ZLIB On CACHE BOOL "Enable zlib (vendored extern/zlib-1.3.2); required by the NBT (Minecraft) parser.")
set(ZLIB_DIR "${CMAKE_SOURCE_DIR}/extern/zlib-1.3.2" CACHE PATH "Path to the vendored zlib source tree.")
set(ENABLE_SULINA Off CACHE BOOL "Compile sulina (Qt 6 + Vulkan, via cgre2).")

# Canvas nodal ImGui et son hote natif. DEFAUT Off : l'option ajoute des cibles
# et un executable dont la plupart des configurations n'ont que faire. Elle
# n'ajoute PLUS d'exigence de reseau -- ses trois tierces sont versionnees sous
# extern/, au meme titre que les dix autres du depot, et la configuration
# aboutit hors ligne dans les deux positions de l'option.
set(ENABLE_CGGRAPH_BOILERPLATE Off CACHE BOOL "Build the ImGui node canvas and the cggraph-boilerplate native host.")
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/extern/imgui-1.89.9" CACHE PATH "Path to the vendored Dear ImGui source tree.")
set(IMGUI_NODE_EDITOR_DIR "${CMAKE_SOURCE_DIR}/extern/imgui-node-editor-0.9.3" CACHE PATH "Path to the vendored imgui-node-editor source tree.")
set(GLFW_DIR "${CMAKE_SOURCE_DIR}/extern/glfw-3.3.10" CACHE PATH "Path to the vendored GLFW source tree.")
set(ENABLE_OPENNURBS On CACHE BOOL "Enable openNURBS (.3dm) import.")
set(EXTERN_OPENNURBS "" CACHE PATH "Path to openNURBS directory.")

# lib3mf (3MF Consortium, BSD-2-Clause) : import .3mf. Bibliotheque PARTAGEE
# uniquement (aucun .a/.lib statique publie en amont) -> le .dll/.so doit etre
# livre a cote de chaque executable, comme OpenNURBS et OCCT.
set(ENABLE_LIB3MF Off CACHE BOOL "Enable lib3mf (.3mf) import.")
set(EXTERN_LIB3MF "" CACHE PATH "Path to the lib3mf prebuilt directory (contains include/ and lib/).")

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
# Couche A de cggraph : moteur de graphe, sans dependance. Sans condition, comme
# les trois ci-dessus -- une option la sortirait des deux chaines de CI.
add_subdirectory(src/cggraph/core)
# Couche B : les adaptateurs. Une seule cible, rangee par domaine, qui depend de
# cgmesh -- c'est elle qui nomme les types de domaine, et le moteur l'ignore.
add_subdirectory(src/cggraph/nodes)
# Modele d'edition : palette et inspecteur derives du registre, sans dessin ni
# dependance tierce. Sans condition, pour la meme raison que les deux ci-dessus.
add_subdirectory(src/cggraph/ui)
if ( WIN32 )
	add_subdirectory(src/cgnet)
endif ()
if ( ENABLE_SINAIA )
	add_subdirectory(src/cgre)
endif ()

# Les tests (googletest) ne sont pas construits pour la cible WebAssembly.
if ( NOT EMSCRIPTEN )
	enable_testing()
	add_subdirectory(test)
endif ()


if ( ENABLE_SINAIA )
	add_subdirectory(sinaia)
endif ()

# maker : module WASM (Emscripten) — cf. maker/CMakeLists.txt et maker/ANALYSE.md.
if ( ENABLE_MAKER )
	add_subdirectory(maker)
endif ()

if ( ENABLE_CGNET_DEMO AND WIN32 )
	add_subdirectory(cgnet_demo)
endif ()

if ( ENABLE_RECONSTRUCTION_CLI )
	add_subdirectory(reconstruction-cli)
endif ()

# Canvas nodal : le canvas lui-meme (commun aux hotes) puis l'hote natif.
#
# NATIF -- sous ENABLE_CGGRAPH_BOILERPLATE, defaut Off : c'est la seule partie du
# chantier qui tire une fenetre et GLFW, dont la plupart des configurations
# n'ont que faire.
#
# WEBASSEMBLY -- SANS OPTION, avec maker, et c'est une consequence de D27. Un
# artefact unique veut dire que maker/web/graph.html fait partie du contrat de
# cet artefact : une option qui retirerait le canvas produirait un maker.js ou
# cette page echouerait au CHARGEMENT, sans que rien dans le build ne l'annonce.
# Le prix -- la taille du module, payee aussi par les six pages qui ne s'en
# servent pas -- est mesure, pas suppose. L'hote web n'est pas cggraph-boilerplate :
# il n'y a ni fenetre ni GLFW dans un Worker, et c'est maker/graph_editor.cpp
# qui le remplace, exactement comme le prevoit l'en-tete du canvas.
if ( EMSCRIPTEN )
	if ( ENABLE_MAKER )
		add_subdirectory(src/cggraph/canvas)
	endif ()
elseif ( ENABLE_CGGRAPH_BOILERPLATE )
	add_subdirectory(src/cggraph/canvas)
	add_subdirectory(cggraph-boilerplate)
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

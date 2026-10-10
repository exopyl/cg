cmake_minimum_required(VERSION 3.18.4)

project(peles)

# ⚠ ENABLE_PELES=ON ROMPT LA PROPRIETE « CONFIGURATION HORS LIGNE » DU DEPOT.
# Partout ailleurs, les dependances sont soit versionnees sous extern/, soit
# pointees par une variable de cache vers un arbre deja present. peles amene
# RmlUi et FreeType par FetchContent, donc par le reseau, a la CONFIGURATION.
# C'est la consequence directe de D-7, et elle est consignee ici plutot que dans
# un document : c'est le fichier qu'on lit quand la configuration echoue.
#
# Le preset ci-linux n'active pas ENABLE_PELES pour cette raison.

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FetchRmlUi)

cg_add_vendored_glfw()

# OpenGL::GL et non `opengl32` : la cible importee vaut opengl32 sous Windows et
# libGL ailleurs. Elle n'est necessaire qu'a peles_platform, qui compile
# RmlUi_Renderer_GL2.cpp -- lequel appelle les entrees GL 1.x DIRECTEMENT, sans
# passer par glad.
find_package(OpenGL REQUIRED)

# ---------------------------------------------------------------------------
#  peles_probe -- sonde de contexte OpenGL
# ---------------------------------------------------------------------------
#
# CIBLE DISTINCTE de l'application `peles`, et permanente. Elle ne lie NI
# wxWidgets NI RmlUi -- ce qu'elle mesure est le contexte, pas une interface.
# C'est aussi ce qui la rend batissable la ou RmlUi ne peut pas etre tire.
#
# Elle est le premier livrable du chantier parce que cgre est lie au profil de
# compatibilite d'OpenGL (cf. src/cgre/surface_program.h). Si glfw ne delivre
# pas un tel contexte sur la machine cible, `peles` sur cgre est impossible.

add_executable(peles_probe probe/main.cpp)

# cgre publie son repertoire d'inclusion ET ses dependances (cgmath, cgimg,
# cgmesh) en PUBLIC : les nommer une seconde fois ici n'ajouterait rien et
# ferait diverger les deux listes a la premiere evolution.
target_link_libraries(peles_probe PRIVATE cgre glfw)

# AUCUNE LIAISON A OpenGL::GL, et c'est delibere : la sonde n'appelle pas une
# seule entree GL directement. Toutes passent par les pointeurs de glad, dont la
# definition vit dans src/cgre/glad/gl.c, donc dans la bibliotheque cgre.
# cggraph-boilerplate, lui, lie OpenGL::GL parce que son main.cpp appelle
# glViewport et glClear a travers l'en-tete GL du systeme.

# ---------------------------------------------------------------------------
#  RmlUi et FreeType
# ---------------------------------------------------------------------------
#
# Place APRES peles_probe : la sonde ne doit rien devoir a RmlUi, et l'ordre le
# montre.

peles_add_rmlui()

# ---------------------------------------------------------------------------
#  LE PARE-FEU D'INCLUSION, TENU PAR L'EDITEUR DE LIENS
# ---------------------------------------------------------------------------
#
# src/cgre/glad/gl.h ne declare pas seulement des pointeurs : il REDEFINIT
# CHAQUE ENTREE GL PAR MACRO (:4345-4346 `#define glPopAttrib glad_glPopAttrib`,
# :4949-4950 `#define glUseProgram glad_glUseProgram`). Regle qui en decoule :
# AUCUNE UNITE DE TRADUCTION N'INCLUT A LA FOIS src/cgre/gl_wrapper.h ET UN
# EN-TETE GL DE RmlUi.
#
#   peles_core      lie cgmesh           -- NE LIE NI glfw NI RmlUi NI cgre
#   peles_view3d    lie peles_core+cgre  -- NE LIE PAS RmlUi
#   peles_platform  lie glfw+RmlUi       -- NE LIE PAS cgre
#   peles_ui        lie peles_core+RmlUi -- NE LIE NI cgre NI glfw
#   peles (exe)     lie les quatre
#
# ⚠ CE SONT QUATRE CIBLES DISTINCTES, ET NON UN DECOUPAGE DE CONFORT :
# c'est LE COMPILATEUR qui doit refuser une inclusion fautive, pas un relecteur.
# Un fichier de core/ qui ecrirait `#include "gl_wrapper.h"` ne compile pas --
# le repertoire d'inclusion de cgre ne lui est pas donne.
#
# C'est la seule chose qui empeche peles de reproduire le cycle de sinaia, ou
# MyGLCanvas et MyFrame vivent dans la MEME cible : le `dynamic_cast<MyFrame*>`
# y compile sans obstacle, six fois. Ici il n'existe pas de type auquel remonter.
#
# ⚠ L'ARETE view3d -> app N'EST PAS UNE ARETE D'EDITION DE LIENS, et le tableau
# ci-dessus ne la couvre donc pas. `app` est l'executable : aucune bibliotheque
# statique ne peut le lier, et un `#include "app/application.h"` ecrit depuis
# view3d/ ne produirait meme pas un symbole non resolu -- application.cpp est
# dans la meme cible finale. CETTE ARETE EST TENUE PAR LE SEUL CHEMIN
# D'INCLUSION : aucune des quatre bibliotheques ne recoit ${CMAKE_CURRENT_SOURCE_DIR},
# donc `app/…`, `platform/…` et `ui/…` sont introuvables depuis elles. Ajouter
# la racine a l'une d'elles rouvrirait le cycle de sinaia en silence.
#
# ⚠ DANS L'EXECUTABLE AUSSI, C'EST LE COMPILATEUR QUI TIENT LA FRONTIERE, et
# pas l'ordre d'inclusion d'app/application.cpp. src/cgre/glad/gl.h:36-37 rend
# `#error` si __gl_h_ est deja defini, et c'est ce que definit tout en-tete GL
# de systeme (um/gl/GL.h:16-20 du SDK Windows, comme celui de Mesa). Le seul
# en-tete qui pourrait entrer en conflit ici, RmlUi_Renderer_GL2.h, est de
# surcroit inatteignable : Backends/ est un chemin d'inclusion PRIVE de
# peles_platform.

add_library(peles_core STATIC
	core/directory_tree.h
	core/directory_tree.cpp
	core/document.h
	core/document.cpp
	core/document_observer.h
	core/file_catalog.h
	core/file_catalog.cpp
	core/file_list.h
	core/file_list.cpp
	core/loader.h
	core/loader.cpp
	core/model_info.h
	core/model_info.cpp
	core/path_text.h
	core/path_text.cpp
	core/splitter_bounds.h
	core/splitter_bounds.cpp
	core/topology_stats.h
	core/topology_stats.cpp
	core/ui_dto.h)
target_include_directories(peles_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core)
target_link_libraries(peles_core PUBLIC cgmesh)
# core/directory_tree.cpp tire <windows.h> pour enumerer les lecteurs : NOMINMAX
# y est pose par la configuration, comme sur les deux autres cibles qui
# l'incluent. Un `#define` en tete d'un seul .cpp est une convention qu'un
# fichier ajoute plus tard ne connait pas.
target_compile_definitions(peles_core PRIVATE NOMINMAX)

add_library(peles_view3d STATIC
	view3d/camera.h
	view3d/camera.cpp
	view3d/picking.h
	view3d/picking.cpp
	view3d/scene_renderer.h
	view3d/scene_renderer.cpp)
target_include_directories(peles_view3d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/view3d)
# ⚠ PAS LA RACINE DE peles ICI. Elle mettrait app/, platform/ et ui/ sur le
# chemin d'inclusion des trois bibliotheques, et `#include "app/application.h"`
# depuis view3d/ compilerait : application.h ne nomme aucun type de view3d,
# platform ou ui autrement que par declaration anticipee, et application.cpp est
# dans l'executable, donc le lien passerait aussi. L'arete view3d -> app n'est
# PAS une arete d'edition de liens -- app est l'executable -- et le chemin
# d'inclusion est le SEUL mecanisme qui la garde.
# cgre en PRIVATE : aucun en-tete de view3d/ ne nomme un type de cgre -- le
# trackball est tenu derriere un pointeur opaque et le maillage derriere une
# declaration anticipee. Le publier donnerait le repertoire d'inclusion de cgre,
# donc gl_wrapper.h, a tout consommateur de peles_view3d.
target_link_libraries(peles_view3d PUBLIC peles_core)
target_link_libraries(peles_view3d PRIVATE cgre)

add_library(peles_platform STATIC
	platform/window_glfw.h
	platform/window_glfw.cpp
	platform/rml_system_interface.h
	platform/rml_system_interface.cpp
	platform/rml_render_interface.h
	platform/rml_render_interface.cpp
	platform/rml_host.h
	platform/rml_host.cpp
	# Le renderer GL2 de RmlUi, compile TEL QUEL depuis l'arbre amont. RmlUi
	# distingue ses *renderers*, « usable as-is by client projects without
	# modifications », de ses *backends*, qui sont des echantillons a recopier :
	# celui-ci est un renderer, et il n'en existe aucune copie locale.
	${PELES_RMLUI_BACKENDS_DIR}/RmlUi_Renderer_GL2.cpp)
target_include_directories(peles_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/platform)
# PRIVATE, et c'est le point : Backends/ contient l'en-tete GL du renderer.
# Le publier laisserait une unite exterieure l'atteindre par accident.
target_include_directories(peles_platform PRIVATE ${PELES_RMLUI_BACKENDS_DIR})
# RmlUi::Core est la SEULE dependance publique, et elle l'est par obligation :
# platform/rml_system_interface.h:3 inclut <RmlUi/Core/SystemInterface.h> et
# derive de Rml::SystemInterface. Tout le reste est un detail d'implementation
# -- glfw3.h n'apparait que dans window_glfw.cpp, derriere un pimpl, et le
# debogueur derriere rml_host.cpp.
target_link_libraries(peles_platform PUBLIC RmlUi::Core)
target_link_libraries(peles_platform PRIVATE glfw RmlUi::Debugger OpenGL::GL)

# ⚠ POSES PAR LA CONFIGURATION, PAS PAR FICHIER. Un `#define` en tete d'un seul
# .cpp est une convention qu'un fichier ajoute plus tard ne connait pas.
#   NOMINMAX          : <windows.h> definit min et max comme macros, et les
#                       en-tetes de RmlUi appellent std::min et std::max --
#                       l'expansion produirait une erreur de syntaxe dont le
#                       diagnostic pointerait l'en-tete tiers, pas la cause.
#   GLFW_INCLUDE_NONE : sans lui, glfw3.h tire son propre en-tete GL, alors que
#                       les entrees GL de ce processus viennent de glad seul.
target_compile_definitions(peles_platform PRIVATE NOMINMAX GLFW_INCLUDE_NONE)

add_library(peles_ui STATIC
	ui/input_event.h
	ui/input_router.h
	ui/input_router.cpp
	ui/panel_splitters.h
	ui/panel_splitters.cpp
	ui/ui_command.h
	ui/ui_controller.h
	ui/ui_controller.cpp
	ui/view_model.h
	ui/view_model.cpp
	ui/viewport_rect.h
	ui/viewport_rect.cpp)
target_include_directories(peles_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ui)
# RmlUi::Core en PRIVATE : ui/viewport_rect.h se contente d'une declaration
# anticipee de Rml::Element, et c'est viewport_rect.cpp qui inclut RmlUi.
target_link_libraries(peles_ui PUBLIC peles_core)
target_link_libraries(peles_ui PRIVATE RmlUi::Core)

add_executable(peles
	main.cpp
	# L'HOTE, DECOUPE PAR ROLE. Une seule unite en faisait 1808 lignes pour un
	# seuil de 800 : la creation, les huit phases, les gestes, les instruments de
	# boucle et les modes « vider et sortir » y etaient melanges, et l'ordre des
	# huit phases -- qui est la conception -- s'y perdait.
	app/application.h
	app/application_internal.h
	app/application.cpp
	app/application_create.cpp
	app/application_scene.cpp
	app/application_loop.cpp
	app/application_modes.cpp
	app/application_splitters.cpp
	app/asset_paths.h
	app/asset_paths.cpp
	# L'INSTRUMENT, SEPARE DE L'HOTE ET DECOUPE PAR NATURE DE MESURE. Les modes
	# de mesure pesent plus que les huit phases qu'ils mesurent : une seule
	# unite en faisait 1814 lignes, pour un seuil de 800 dans ce depot.
	#
	# La ligne de coupe est CE QUE LA MESURE OBSERVE, et elle se lit dans les
	# listes d'inclusions : dump_state est la seule a inclure gl_wrapper.h,
	# dump_oracles la seule a n'inclure ni RmlUi ni GL -- ce qui est exactement
	# la propriete qui rend --dump-tree/--dump-files/--dump-info executables en
	# CI. Les declarations restent dans le seul app/dump_modes.h.
	app/dump_modes.h
	app/dump_state.cpp
	app/dump_geometry.cpp
	app/dump_gestures.cpp
	app/dump_effects.cpp
	app/dump_session.cpp
	app/dump_oracles.cpp
	# gl_state_fence est du COTE cgre du pare-feu : il appelle glPushAttrib,
	# donc il inclut gl_wrapper.h.
	app/gl_state_fence.h
	app/gl_state_fence.cpp)
target_include_directories(peles PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app)
# LA RACINE DE peles, ET SEULEMENT POUR L'EXECUTABLE. Elle sert un seul besoin :
# `#include "view3d/camera.h"` dans app/application.cpp. src/cgmesh/camera.h
# existe et cgmesh publie son repertoire, donc un `#include "camera.h"` ecrit
# depuis app/ resout vers la camera de cgmesh et le diagnostic parle d'un type
# incomplet sans nommer la collision. Les unites de view3d/, elles, n'ont pas
# besoin du prefixe : la regle « repertoire du fichier incluant d'abord » les
# fait resoudre vers view3d/camera.h.
target_include_directories(peles PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(peles PRIVATE peles_core peles_view3d peles_platform peles_ui)
# cgre est nomme ICI, et pas herite de peles_view3d : sept unites de app/
# incluent gl_wrapper.h, app/application_create.cpp appelle cgre::SetLogSink, et
# app/application.cpp comme app/gl_state_fence.cpp appellent cgre::Logf. Une
# dependance utilisee doit etre declaree par celui qui l'utilise -- sans quoi
# rendre cgre prive chez le voisin casserait cette cible a distance.
target_link_libraries(peles PRIVATE cgre)
# cgimg est nomme ICI pour la meme raison : app/application_create.cpp decode
# l'icone de la fenetre avec Img. La dependance arrive aussi par cgmesh, mais
# celui qui l'utilise la declare -- sans quoi un changement de visibilite chez le
# voisin casserait cette cible a distance.
target_link_libraries(peles PRIVATE cgimg)

# Memes definitions, meme raison que pour peles_platform, et NEUF unites sont
# concernees -- pas deux. Tirent <windows.h> par gl_wrapper.h puis des en-tetes
# de RmlUi : app/application.cpp, app/application_create.cpp,
# app/application_scene.cpp, app/application_loop.cpp, app/application_modes.cpp,
# app/dump_state.cpp et app/gl_state_fence.cpp. Le tirent directement :
# app/asset_paths.cpp et app/dump_session.cpp (<psapi.h> exige <windows.h>
# devant lui).
target_compile_definitions(peles PRIVATE NOMINMAX GLFW_INCLUDE_NONE)

# L'EMPREINTE MEMOIRE DU PROCESSUS, POUR --reselect. GetProcessMemoryInfo vit
# dans psapi ; sous Linux la meme mesure se lit dans /proc/self/statm, sans
# bibliotheque a lier. C'est la seule dependance systeme de l'instrument.
if (WIN32)
	target_link_libraries(peles PRIVATE psapi)
endif ()

# QUATRIEME NIVEAU de la resolution des assets : le chemin compile vers l'arbre
# source. Les trois premiers -- --assets, PELES_ASSETS_DIR, et le repertoire de
# l'executable -- sont resolus a l'execution.
target_compile_definitions(peles PRIVATE
	PELES_ASSETS_DIR_DEFAULT="${PROJECT_SOURCE_DIR}/assets")

# TROISIEME NIVEAU : les assets a cote de l'executable. Meme modele que
# sinaia/CMakeLists.txt:135-143. Le repertoire est copie en entier -- il ne
# contient que des fichiers lus a l'execution, pas d'outillage.
add_custom_command(TARGET peles POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E ${CG_COPY_DIRECTORY}
	"${PROJECT_SOURCE_DIR}/assets"
	"$<TARGET_FILE_DIR:peles>/assets"
	COMMENT "Copying peles assets to $<TARGET_FILE_DIR:peles>/assets")

# ---------------------------------------------------------------------------
#  Dependances implicites de cgmesh
# ---------------------------------------------------------------------------
#
# cgmesh importe OpenNURBS, lib3mf et OCCT de facon IMPLICITE : sans leurs
# bibliotheques partagees a cote de l'executable, il ne demarre pas. Memes blocs
# que ceux de la cible de test et de cggraph-boilerplate.

foreach(_peles_runtime_target peles_probe peles)
	if (WIN32 AND ENABLE_OPENNURBS)
		add_custom_command(TARGET ${_peles_runtime_target} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"${EXTERN_OPENNURBS}/lib/vc14x_x64/OpenNURBS.dll"
				"$<TARGET_FILE_DIR:${_peles_runtime_target}>")
	endif()

	if (UNIX AND ENABLE_OPENNURBS)
		add_custom_command(TARGET ${_peles_runtime_target} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"${EXTERN_OPENNURBS}/lib/linux_x64/libOpenNURBS.so"
				"$<TARGET_FILE_DIR:${_peles_runtime_target}>")
	endif()

	if (WIN32 AND ENABLE_LIB3MF)
		add_custom_command(TARGET ${_peles_runtime_target} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"${EXTERN_LIB3MF}/bin/lib3mf.dll"
				"$<TARGET_FILE_DIR:${_peles_runtime_target}>")
	endif()

	if (WIN32 AND ENABLE_OCCT)
		add_custom_command(TARGET ${_peles_runtime_target} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E ${CG_COPY_DIRECTORY}
				"${EXTERN_OCCT}/bin"
				"$<TARGET_FILE_DIR:${_peles_runtime_target}>")
	endif()
endforeach()

# ---------------------------------------------------------------------------
#  LES DEUX ORACLES, COMPARES PAR ctest
# ---------------------------------------------------------------------------
#
# ⚠ AVANT CE BLOC, LES DEUX FICHIERS ATTENDUS ETAIENT DES ORACLES MANUELS :
# personne ne faisait la comparaison que leur `.gitattributes` decrit. Un
# attendu qu'aucune commande ne lit ne protege rien -- il enregistre une sortie,
# il ne la controle pas.
#
# ⚠ LES DEUX SEULS MODES DE peles EXECUTABLES SANS CONTEXTE GL : ils n'appellent
# que peles_core, ne lisent aucun asset et n'ouvrent aucune fenetre. C'est ce qui
# les rend lançables par ctest sur une machine sans interface graphique -- et
# c'est aussi pourquoi ils sont les seuls ici.
#
# ⚠ enable_testing() EST APPELE PAR LE CMakeLists RACINE, avant
# add_subdirectory(peles). L'ordre n'est pas libre : add_test hors d'un arbre ou
# les tests sont actives est ignore en silence.
if (NOT EMSCRIPTEN AND EXISTS "${CMAKE_SOURCE_DIR}/test/data/peles/fixture")
	foreach(_peles_dump tree files)
		add_test(NAME peles_dump_${_peles_dump}_oracle
			COMMAND ${CMAKE_COMMAND}
				-DPELES_EXE=$<TARGET_FILE:peles>
				-DMODE=--dump-${_peles_dump}
				-DFIXTURE=${CMAKE_SOURCE_DIR}/test/data/peles/fixture
				-DEXPECTED=${CMAKE_SOURCE_DIR}/test/data/peles/expected_dump_${_peles_dump}.txt
				-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/oracle/dump_${_peles_dump}.txt
				-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompareDumpOracle.cmake)
	endforeach()

	# ⚠ --dump-info PASSE UN CHEMIN RELATIF ET UN REPERTOIRE DE TRAVAIL, la ou
	# les deux autres passent un chemin absolu. La raison est dans la sortie :
	# la ligne « Path » rend le chemin TEL QU'IL A ETE DONNE -- c'est la
	# quatorzieme ligne, celle qui remplace l'infobulle de l'hote historique.
	# Un chemin absolu y ferait un oracle propre a une machine.
	add_test(NAME peles_dump_info_oracle
		COMMAND ${CMAKE_COMMAND}
			-DPELES_EXE=$<TARGET_FILE:peles>
			-DMODE=--dump-info
			-DWORKDIR=${CMAKE_SOURCE_DIR}
			-DFIXTURE=test/data/peles/info/panels.obj
			-DEXPECTED=${CMAKE_SOURCE_DIR}/test/data/peles/expected_dump_info.txt
			-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/oracle/dump_info.txt
			-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompareDumpOracle.cmake)
endif ()

install(TARGETS peles_probe peles DESTINATION .)
install(DIRECTORY assets DESTINATION .)

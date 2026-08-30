cmake_minimum_required(VERSION 3.18.4)

project(libcggraph_canvas)

# ---------------------------------------------------------------------------
#  Dependances tierces -- VERSIONNEES SOUS extern/, comme les dix autres
# ---------------------------------------------------------------------------
# Ce repertoire est le SEUL du projet racine qui depende d'ImGui, et il n'est
# atteint que sous ENABLE_CGGRAPH_BOILERPLATE, dont le defaut est Off.
#
# Les sources d'imgui et d'imgui-node-editor sont dans l'arbre, sous
# extern/imgui-<version> et extern/imgui-node-editor-<version> : la
# configuration n'ouvre AUCUNE connexion, dans les deux positions de l'option.
# Seuls les fichiers compiles ou inclus sont embarques -- ni exemples, ni
# documentation, ni backends autres que ceux dont l'hote natif se sert.
#
# Les deux bibliotheques sont sous licence MIT.
#
# ⚠ AVANT DE METTRE A JOUR L'UNE DES DEUX, LIRE CECI : LE COUPLE DE VERSIONS
# N'EST PAS LIBRE. imgui-node-editor v0.9.3, sa derniere etiquette publiee,
# embarque un imgui_extra_math.inl reste en 0.9.1 qui definit operator== et
# operator!= sur ImVec2 ; imgui les definit lui-meme a partir de la 1.90, et les
# deux definitions se percutent (C2084, mesure faite ici avec la 1.90.1). D'ou
# imgui 1.89.9. Monter imgui sans monter le node editor -- ou sans retirer ces
# deux operateurs de imgui_extra_math.inl -- casse la compilation du canvas.
# ⚠ Ce n'est pas la version que `vecna` recupere de son cote (v1.90.1) : les
# deux arbres etant des projets CMake distincts, les deux copies ne se
# rencontrent jamais -- mais le jour ou l'un absorberait l'autre, c'est ce
# couple-la qu'il faudrait rejuger, non le corriger a la hate.
foreach(dir IMGUI_DIR IMGUI_NODE_EDITOR_DIR)
    if (NOT EXISTS "${${dir}}")
        message(FATAL_ERROR "${dir} introuvable : ${${dir}}")
    endif()
endforeach()

# Coeur d'ImGui, sans aucun backend : les backends appartiennent a l'HOTE, et
# c'est ce qui garde le canvas commun aux hotes (D4). Une cible qui tirerait ici
# imgui_impl_glfw ferait du canvas un canvas de bureau.
add_library(imgui_core STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp)
target_include_directories(imgui_core PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)

add_library(imgui_node_editor STATIC
    ${IMGUI_NODE_EDITOR_DIR}/imgui_node_editor.cpp
    ${IMGUI_NODE_EDITOR_DIR}/imgui_node_editor_api.cpp
    ${IMGUI_NODE_EDITOR_DIR}/imgui_canvas.cpp
    ${IMGUI_NODE_EDITOR_DIR}/crude_json.cpp)
target_include_directories(imgui_node_editor PUBLIC ${IMGUI_NODE_EDITOR_DIR})
target_link_libraries(imgui_node_editor PUBLIC imgui_core)

# Code tiers : ses avertissements ne sont pas les notres, et les melanger aux
# notres rend les notres invisibles.
if(MSVC)
    target_compile_options(imgui_core PRIVATE /W0)
    target_compile_options(imgui_node_editor PRIVATE /W0)
else()
    target_compile_options(imgui_core PRIVATE -w)
    target_compile_options(imgui_node_editor PRIVATE -w)
endif()

# ---------------------------------------------------------------------------
#  Le canvas -- une seule implementation, aucun hote
# ---------------------------------------------------------------------------
file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)

add_library(cggraph_canvas STATIC ${HEADER_FILES} ${SRC_FILES})

target_link_libraries(cggraph_canvas cggraph_ui cggraph imgui_node_editor)

install(TARGETS cggraph_canvas DESTINATION .)

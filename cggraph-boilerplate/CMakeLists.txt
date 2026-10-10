cmake_minimum_required(VERSION 3.18.4)

project(cggraph-boilerplate)

# Hote NATIF du canvas. Sous ENABLE_CGGRAPH_BOILERPLATE uniquement, comme le canvas
# lui-meme : la fenetre est le seul morceau propre a un hote, et c'est aussi le
# seul qui tire GLFW.
#
# GLFW passe par cg_add_vendored_glfw(), definie dans cmake/CgVendoredGlfw.cmake
# et incluse par le CMakeLists racine. Elle porte l'arbre versionne, l'extinction
# des quatre extras d'amont et le relevement de borne de politique qu'exige
# CMake 4 -- avec leurs raisons.
#
# Elle est IDEMPOTENTE : ENABLE_CGGRAPH_BOILERPLATE et ENABLE_PELES tirent la
# meme fenetre et se combinent dans n'importe quel ordre.
cg_add_vendored_glfw()

# Les backends d'ImGui sont a l'HOTE, jamais au canvas : c'est le seul endroit
# ou le choix de la fenetre et celui de l'API graphique sont faits. Les sources
# viennent de la meme copie versionnee que le coeur, cote src/cggraph/canvas.
add_executable(cggraph-boilerplate
    main.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)

target_link_libraries(cggraph-boilerplate cggraph_canvas cggraph_ui cggraph_nodes cggraph cgmesh cgimg cgmath glfw)

# main.cpp appelle glViewport, glClear et glClearColor DIRECTEMENT : l'hote
# possede le contexte, c'est donc lui qui lie l'implementation OpenGL. Le
# backend imgui_impl_opengl3 ne suffit pas -- il charge ses propres pointeurs de
# fonction et n'exige rien a l'edition de liens.
#
# OpenGL::GL et non `opengl32` : la cible importee vaut opengl32 sous Windows et
# libGL sous Linux. La forme conditionnelle precedente ne nommait que Windows, et
# l'hote natif ne LIAIT PAS sous GCC -- defaut reste invisible tant qu'aucune
# chaine ne batissait cette cible.
find_package(OpenGL REQUIRED)
target_link_libraries(cggraph-boilerplate OpenGL::GL)

# cgmesh importe OpenNURBS, lib3mf et OCCT de facon IMPLICITE : sans elles a
# cote de l'exe, il ne demarre pas. Memes blocs que ceux de la cible de test.
if (WIN32 AND ENABLE_OPENNURBS)
    add_custom_command(TARGET cggraph-boilerplate POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_OPENNURBS}/lib/vc14x_x64/OpenNURBS.dll"
            "$<TARGET_FILE_DIR:cggraph-boilerplate>")
endif()

if (UNIX AND ENABLE_OPENNURBS)
    add_custom_command(TARGET cggraph-boilerplate POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_OPENNURBS}/lib/linux_x64/libOpenNURBS.so"
            "$<TARGET_FILE_DIR:cggraph-boilerplate>")
endif()

if (WIN32 AND ENABLE_LIB3MF)
    add_custom_command(TARGET cggraph-boilerplate POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_LIB3MF}/bin/lib3mf.dll"
            "$<TARGET_FILE_DIR:cggraph-boilerplate>")
endif()

if (WIN32 AND ENABLE_OCCT)
    add_custom_command(TARGET cggraph-boilerplate POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E ${CG_COPY_DIRECTORY}
            "${EXTERN_OCCT}/bin"
            "$<TARGET_FILE_DIR:cggraph-boilerplate>")
endif()

install(TARGETS cggraph-boilerplate DESTINATION .)

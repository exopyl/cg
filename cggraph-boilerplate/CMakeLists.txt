cmake_minimum_required(VERSION 3.18.4)

project(cggraph-boilerplate)

# Hote NATIF du canvas. Sous ENABLE_CGGRAPH_BOILERPLATE uniquement, comme le canvas
# lui-meme : la fenetre est le seul morceau propre a un hote, et c'est aussi le
# seul qui tire GLFW.
#
# GLFW est versionne sous extern/glfw-<version>, licence zlib/libpng. N'en sont
# embarques que le projet CMake, include/ et src/ : ni exemples, ni tests, ni
# documentation. Meme version que celle deja retenue par vecna dans ce depot --
# deux versions de GLFW dans un meme arbre seraient une source de confusion sans
# contrepartie.
if (NOT EXISTS "${GLFW_DIR}")
    message(FATAL_ERROR "GLFW_DIR introuvable : ${GLFW_DIR}")
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

# Le CMakeLists amont de GLFW 3.3.10 declare `cmake_minimum_required(3.0...3.20)`
# et CMake 4 refuse une borne basse anterieure a 3.5. La borne est relevee ICI,
# pour ce sous-arbre seulement : editer le fichier amont ferait de la copie
# versionnee un fork a re-appliquer a chaque montee de version.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory(${GLFW_DIR} ${CMAKE_CURRENT_BINARY_DIR}/glfw EXCLUDE_FROM_ALL)
unset(CMAKE_POLICY_VERSION_MINIMUM)

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

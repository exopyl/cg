cmake_minimum_required(VERSION 3.18.4)

project(cgre)

set(sources
	glad/gl.c
    gl_wrapper.h
    capabilities_manager.cpp
    diagnostics.cpp
    examinator_trackball.cpp
    gl_program.cpp
    surface_program.cpp
    material_renderer.cpp
    mesh_renderer.cpp
    vertex_buffer_manager.cpp
    widgets_renderer.cpp
    )

set(headers
	glad/gl.h
    cgre.h
    capabilities_manager.h
    diagnostics.h
    examinator_trackball.h
    gl_program.h
    surface_program.h
    material_renderer.h
    mesh_renderer.h
    vertex_buffer_manager.h
    widgets_renderer.h
    )

# Le chargeur WGL est propre a Windows : il n'a pas d'equivalent ailleurs et ses
# en-tetes exigent <windows.h>. Sur les autres plateformes, glad/gl.c ouvre
# libGL par dlopen et resout glXGetProcAddressARB par dlsym.
if (WIN32)
    list(APPEND sources glad/wgl.c)
    list(APPEND headers glad/wgl.h)
endif ()

include_directories(.)
add_library(cgre STATIC ${headers} ${sources})
target_include_directories(cgre PUBLIC ${CMAKE_SOURCE_DIR}/extern)

# Le repertoire du module, sur la CIBLE et non par include_directories(.) : cette
# derniere a une portee REPERTOIRE, donc elle ne suit pas la bibliotheque chez ses
# consommateurs. gl_wrapper.h tire "glad/gl.h" (et "glad/wgl.h" sous Windows) :
# sans ce chemin, toute unite exterieure incluant un en-tete de cgre echoue sur
# ces inclusions. C'est ce qui empechait la cible TU de compiler le premier test
# du module.
target_include_directories(cgre PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# glad/gl.c appelle dlopen/dlsym/dlclose pour ouvrir libGL. Sur glibc >= 2.34
# ces symboles sont dans la libc et CMAKE_DL_LIBS est vide ; la formulation
# reste necessaire pour les plateformes ou ils vivent dans libdl.
target_link_libraries(cgre PRIVATE ${CMAKE_DL_LIBS})

# DEPENDANCES DECLAREES. cgre inclut cgmath, cgimg et cgmesh, et n'en liait
# aucun : il ne compilait que parce que ses inclusions etaient RELATIVES, donc
# resolues depuis le repertoire du fichier qui les ecrit. Les declarer donne au
# passage le repertoire d'inclusion public de ces modules, sans lequel
# `#include <cgmath/cgmath.h>` ne resout pas.
#
# cgre est bati sous ENABLE_CGRE, que ENABLE_SINAIA implique et que le preset
# ci-linux active : un defaut de configuration se voit desormais dans la CI
# Linux, pas seulement dans une construction Windows complete.
target_link_libraries(cgre PUBLIC cg::cgmath cg::cgimg cgmesh)
target_link_libraries(cgmesh cgimg)

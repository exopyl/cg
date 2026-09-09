cmake_minimum_required(VERSION 3.18.4)

project(cgre)

set(sources
	glad/gl.c
	glad/wgl.c
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
	glad/wgl.h
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

include_directories(.)
add_library(cgre STATIC ${headers} ${sources})
target_include_directories(cgre PUBLIC ${CMAKE_SOURCE_DIR}/extern)

# Le repertoire du module, sur la CIBLE et non par include_directories(.) : cette
# derniere a une portee REPERTOIRE, donc elle ne suit pas la bibliotheque chez ses
# consommateurs. gl_wrapper.h tire "glad/wgl.h", qui tire lui-meme "glad/gl.h" :
# sans ce chemin, toute unite exterieure incluant un en-tete de cgre echoue sur le
# second. C'est ce qui empechait la cible TU de compiler le premier test du module.
target_include_directories(cgre PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# DEPENDANCES DECLAREES. cgre inclut cgmath, cgimg et cgmesh, et n'en liait
# aucun : il ne compilait que parce que ses inclusions etaient RELATIVES, donc
# resolues depuis le repertoire du fichier qui les ecrit. Les declarer donne au
# passage le repertoire d'inclusion public de ces modules, sans lequel
# `#include <cgmath/cgmath.h>` ne resout pas.
#
# ⚠ cgre n'est bati que sous ENABLE_SINAIA : un defaut de configuration y reste
# invisible dans toute construction ordinaire du depot.
target_link_libraries(cgre PUBLIC cg::cgmath cg::cgimg cgmesh)
target_link_libraries(cgmesh cgimg)

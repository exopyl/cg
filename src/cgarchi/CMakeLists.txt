cmake_minimum_required(VERSION 3.18.4)

project(libcgarchi)

# Architecture gothique : remplage parametrique, region de pierre, fusion des
# barres de moulure, formes parametriques gothiques et leur serialisation.
# Toutes les sources se compilent aussi sous Emscripten (maker lie cette cible) :
# aucune n'est reservee au natif.
file(GLOB SRC_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/*.h)

add_library(cgarchi STATIC ${HEADER_FILES} ${SRC_FILES})

# Inclusions : `#include <cgarchi/x.h>` depuis partout, comme cgmath et cgimg.
# BUILD_INTERFACE seul : le module n'installe aucun en-tete.
target_include_directories(cgarchi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>)
target_compile_features(cgarchi PUBLIC cxx_std_17)

# PUBLIC : les en-tetes de cgarchi exposent Mesh, Polygon2, Profile2D,
# ParameterizedMesh... Les en-tetes de cgmesh s'incluent par leur nom seul
# ("mesh.h"), via le repertoire PUBLIC de cgmesh.
#
# nlohmann/json et Clipper2 ne sont pas declares ici : ils arrivent par cgmesh,
# qui expose extern/ en PUBLIC SYSTEM et compile les sources de Clipper2. Les
# threads de la fusion parallele arrivent de meme (Threads::Threads, lie a
# cgmesh).
target_link_libraries(cgarchi PUBLIC cgmesh)

add_library(cg::cgarchi ALIAS cgarchi)

install(TARGETS cgarchi DESTINATION .)

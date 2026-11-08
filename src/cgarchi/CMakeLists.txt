cmake_minimum_required(VERSION 3.18.4)

project(libcgarchi)

# Architecture gothique : remplage parametrique, region de pierre, fusion des
# barres de moulure, formes parametriques gothiques et leur serialisation.
# Architecture classique : les cinq ordres de Vignole (classical_*).
# Toutes les sources se compilent aussi sous Emscripten (maker lie cette cible) :
# aucune n'est reservee au natif.
file(GLOB SRC_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/*.h)

add_library(cgarchi STATIC ${HEADER_FILES} ${SRC_FILES})

# Donnees des cinq ordres (Vignole), embarquees dans le binaire : les formes du
# catalogue parametrique ne lisent aucun fichier (parametric_catalog.h). Le JSON
# est la copie a l'identique de la source documentaire ; la source C++ qui le
# porte est regeneree quand il change.
set(CGARCHI_ORDERS_JSON "${PROJECT_SOURCE_DIR}/data/cinq_ordres.json")
set(CGARCHI_ORDERS_CPP  "${CMAKE_CURRENT_BINARY_DIR}/generated/classical_orders_data.cpp")
add_custom_command(
    OUTPUT  "${CGARCHI_ORDERS_CPP}"
    COMMAND ${CMAKE_COMMAND}
            -DINPUT=${CGARCHI_ORDERS_JSON}
            -DOUTPUT=${CGARCHI_ORDERS_CPP}
            -DNAMESPACE=cgarchi
            -DFUNCTION=ClassicalOrdersJson
            -DHEADER=cgarchi/classical_orders_data.h
            -P "${PROJECT_SOURCE_DIR}/embed_file.cmake"
    DEPENDS "${CGARCHI_ORDERS_JSON}" "${PROJECT_SOURCE_DIR}/embed_file.cmake"
    COMMENT "Embedding data/cinq_ordres.json"
    VERBATIM)
target_sources(cgarchi PRIVATE "${CGARCHI_ORDERS_CPP}")

# Document du temple d'Hephaistos ("monument/v6"), embarque de meme. Le JSON est
# la sortie de data/temple_hephaistos_from_readme.py sur le README de
# l'utilisateur, hors du depot : le script se lance a la main, jamais par la
# construction.
set(CGARCHI_MONUMENT_JSON "${PROJECT_SOURCE_DIR}/data/temple_hephaistos.json")
set(CGARCHI_MONUMENT_CPP  "${CMAKE_CURRENT_BINARY_DIR}/generated/classical_monument_data.cpp")
add_custom_command(
    OUTPUT  "${CGARCHI_MONUMENT_CPP}"
    COMMAND ${CMAKE_COMMAND}
            -DINPUT=${CGARCHI_MONUMENT_JSON}
            -DOUTPUT=${CGARCHI_MONUMENT_CPP}
            -DNAMESPACE=cgarchi
            -DFUNCTION=TempleHephaistosJson
            -DHEADER=cgarchi/classical_monument_data.h
            -P "${PROJECT_SOURCE_DIR}/embed_file.cmake"
    DEPENDS "${CGARCHI_MONUMENT_JSON}" "${PROJECT_SOURCE_DIR}/embed_file.cmake"
    COMMENT "Embedding data/temple_hephaistos.json"
    VERBATIM)
target_sources(cgarchi PRIVATE "${CGARCHI_MONUMENT_CPP}")

# Inclusions : `#include <cgarchi/x.h>` depuis partout, comme cgmath et cgimg.
# BUILD_INTERFACE seul : le module n'installe aucun en-tete.
target_include_directories(cgarchi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>)
target_compile_features(cgarchi PUBLIC cxx_std_17)

# Avertissements portes par la cible, sans -Werror : le niveau ne fuit pas vers
# les consommateurs (PRIVATE), et un compilateur plus recent ne casse pas le
# build sur un avertissement nouveau.
target_compile_options(cgarchi PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)

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

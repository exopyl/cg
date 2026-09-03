cmake_minimum_required(VERSION 3.18.4)

project(libcgmath)

file(GLOB SRC_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/*.h)

add_library(cgmath STATIC ${HEADER_FILES} ${SRC_FILES})

# Third-party headers under <repo>/extern (e.g. nlohmann/json.hpp), used only
# by cgmath's own .cpp (architecture_gothic_io.cpp) -> PRIVATE so the dependency
# does not leak to consumers. Path relative to cgmath's own location so this
# works whether built from the repo-root CMakeLists or as a subproject of vecna/.
target_include_directories(cgmath PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../extern)

# REPERTOIRE D'INCLUSION PUBLIC. Sans lui, les consommateurs ecrivent
# `#include "../cgmath/TVector3.h"` -- plus de soixante fois dans quatre modules
# -- et le chemin depend donc de l'emplacement du fichier qui inclut. Avec lui,
# `#include <cgmath/TVector3.h>` vaut de partout, y compris hors de l'arbre.
#
# BUILD_INTERFACE seul : le module n'installe aucun en-tete (cf. l'install
# ci-dessous), donc promettre un chemin apres installation serait mentir. Le
# jour ou les en-tetes seront installes, un INSTALL_INTERFACE viendra ici.
target_include_directories(cgmath PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>)

# Le standard est porte par la CIBLE et non par une variable globale : un
# consommateur hors de cette racine -- vecna en compile une partie en C++20 --
# herite alors du minimum exige au lieu de le deviner.
target_compile_features(cgmath PUBLIC cxx_std_17)

# Alias d'espace de noms : `cg::cgmath` se lit comme une cible importee, et une
# faute de frappe echoue a la configuration au lieu d'inventer un `-lcgmath`.
add_library(cg::cgmath ALIAS cgmath)

install(TARGETS cgmath DESTINATION .)

cmake_minimum_required(VERSION 3.18.4)

project(libcggraph_ui)

# Le MODELE d'edition -- palette, inspecteur, commandes --, et AUCUN dessin.
# Cette cible ne connait ni ImGui, ni fenetre, ni contexte graphique : c'est ce
# qui la rend testable et ce qui la garde commune aux deux hotes (D4). Le canvas
# qui la dessine vit dans src/cggraph/canvas, sous option, parce qu'il est le
# seul a dependre d'une bibliotheque tierce.
#
# Sans condition, comme cggraph et cggraph_nodes : une option la sortirait des
# deux chaines de CI, et c'est ici que vit la generation depuis le registre --
# la propriete que l'etape 4 existe pour tenir.
file(GLOB SRC_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/*.h)

add_library(cggraph_ui STATIC ${HEADER_FILES} ${SRC_FILES})

target_link_libraries(cggraph_ui cggraph_nodes cggraph)

install(TARGETS cggraph_ui DESTINATION .)

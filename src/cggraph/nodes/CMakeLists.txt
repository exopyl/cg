cmake_minimum_required(VERSION 3.18.4)

project(libcggraph_nodes)

# UNE seule cible pour toute la couche des adaptateurs, rangee par domaine en
# sous-repertoires. La decouper en cgmesh_nodes / cgimg_nodes n'isolerait AUCUNE
# dependance : src/cgmesh/CMakeLists.txt fait deja target_link_libraries(cgmesh
# cgimg), donc tout consommateur de cgmesh tire cgimg. Ce serait deux cibles a
# maintenir pour un allegement nul.
#
# Le decoupage ulterieur reste mecanique tant qu'un adaptateur n'inclut pas un
# adaptateur d'un autre sous-repertoire. C'est la seule discipline que le
# rangement impose ; ce qui est commun aux domaines vit a la RACINE.
file(GLOB SRC_FILES
	${PROJECT_SOURCE_DIR}/*.cpp
	${PROJECT_SOURCE_DIR}/flow/*.cpp
	${PROJECT_SOURCE_DIR}/img/*.cpp
	${PROJECT_SOURCE_DIR}/io/*.cpp
	${PROJECT_SOURCE_DIR}/mesh/*.cpp
	${PROJECT_SOURCE_DIR}/shapes/*.cpp
	${PROJECT_SOURCE_DIR}/svg/*.cpp
	${PROJECT_SOURCE_DIR}/text/*.cpp)
file(GLOB HEADER_FILES
	${PROJECT_SOURCE_DIR}/*.h
	${PROJECT_SOURCE_DIR}/flow/*.h
	${PROJECT_SOURCE_DIR}/img/*.h
	${PROJECT_SOURCE_DIR}/io/*.h
	${PROJECT_SOURCE_DIR}/mesh/*.h
	${PROJECT_SOURCE_DIR}/shapes/*.h
	${PROJECT_SOURCE_DIR}/svg/*.h
	${PROJECT_SOURCE_DIR}/text/*.h)

# Noeuds d'ANALYSE : NATIFS SEULEMENT.
#
# Ils emballent des corps de cgmesh que la cible WASM ne compile pas -- la liste
# EMSCRIPTEN de src/cgmesh/CMakeLists.txt n'en retient qu'un sous-ensemble. Les
# compiler ici et les enregistrer au catalogue ferait echouer le LIEN de maker
# sur des symboles introuvables ; « compile » et « lie » sont deux affirmations
# distinctes, et l'etape 7 a paye la difference.
#
# La meme condition est portee par catalog.cpp, qui n'enregistre pas ces entrees
# sous __EMSCRIPTEN__. Les deux sens de derive entre ces deux listes sont
# BRUYANTS : une source retiree dont l'enregistrement resterait casse le lien ;
# un enregistrement retire dont la source resterait produit un objet que
# personne ne tire, et la taille de maker.wasm ne bouge pas.
#
# Le critere d'exclusion est la PORTABILITE, pas un domaine : c'est pourquoi ces
# fichiers restent dans mesh/ et ne recoivent pas leur propre sous-repertoire.
set(NATIVE_ONLY_SRC
	${PROJECT_SOURCE_DIR}/mesh/ambient_occlusion.cpp
	${PROJECT_SOURCE_DIR}/mesh/colormap.cpp
	${PROJECT_SOURCE_DIR}/mesh/convex_hull.cpp
	${PROJECT_SOURCE_DIR}/mesh/curvature.cpp
	${PROJECT_SOURCE_DIR}/mesh/icp_align.cpp
	${PROJECT_SOURCE_DIR}/mesh/thickness.cpp
	# Decomposition multi-objets : vmeshes_io.cpp n'est pas dans la liste
	# EMSCRIPTEN de cgmesh, et il tire 3ds / gltf / occt / lib3mf derriere lui.
	${PROJECT_SOURCE_DIR}/mesh/load_parts.cpp)
# img/ N'EST PAS dans cette liste non plus, et c'est verifiable plutot que
# suppose : les quatre corps qu'il emballe -- image_relief.cpp,
# image_pixel_blocks.cpp, image_region_pipeline.cpp, image_vectorization.cpp --
# figurent tous dans la liste EMSCRIPTEN de src/cgmesh/CMakeLists.txt, et cgimg
# est globe en entier sans branche EMSCRIPTEN. Les deux chaines image se
# compilent ET SE LIENT sous Emscripten ; c'est d'ailleurs pour le web qu'elles
# ont ete ecrites, les pages relief.html et pixels.html de maker les appellent.
#
# flow/ N'EST PAS dans cette liste, et c'est le fait a retenir de l'etape 9 pour
# la cible web : les trois constructions de flux n'emballent aucun corps de
# cgmesh. Elles se compilent ET SE LIENT sous Emscripten -- verifie sur
# l'artefact maker, pas deduit du fait qu'elles compilent.
if (EMSCRIPTEN)
	list(REMOVE_ITEM SRC_FILES ${NATIVE_ONLY_SRC})
endif()

add_library(cggraph_nodes STATIC ${HEADER_FILES} ${SRC_FILES})

target_link_libraries(cggraph_nodes cggraph cgmesh)

install(TARGETS cggraph_nodes DESTINATION .)

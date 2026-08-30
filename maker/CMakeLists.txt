# ---------------------------------------------------------------------------
# maker : module WebAssembly (Emscripten) exposant le moteur natif au navigateur.
#
# Produit maker.js (glue ES6) + maker.wasm : un pont Embind vers la logique
# geometrique du moteur natif (surfaces parametriques, extrusion SVG, L-systemes).
# La page web de maker/web/ le charge et embarque Online3DViewer comme moteur
# de rendu (cf. maker/ANALYSE.md).
#
# Chaine de dependances : les trois modules natifs (cibles ajoutees par le
# CMakeLists racine). Aucune lib lourde (OpenNURBS/OCCT/ONNX/Poisson/zlib) :
# le preset maker-wasm et le garde EMSCRIPTEN du CMakeLists racine les laissent
# OFF, cf. maker/ANALYSE.md sec.9.
#
# Configuration : depuis la racine du depot,
#     emcmake cmake --preset maker-wasm
#     cmake --build --preset maker-wasm
# (ou `cmake --preset maker-wasm` seul si EMSDK est deja actif dans le shell).
# ---------------------------------------------------------------------------

if (NOT EMSCRIPTEN)
    message(FATAL_ERROR
        "La cible 'maker' ne se compile qu'avec Emscripten.\n"
        "  emcmake cmake --preset maker-wasm")
endif()

# UN SEUL artefact (D27) : les trois facades Embind -- les formes parametriques
# (wasm_api.cpp), la couche nodale procedurale (graph_api.cpp) et l'hote ImGui
# du canvas nodal (graph_editor.cpp) -- vivent dans le meme maker.js. Ce sont
# les INSTANCES qui sont deux (D17) : les six pages chargent la leur sur le
# thread UI, graph.html charge la sienne dans un Web Worker.
#
# imgui_impl_opengl3.cpp est ici et non dans src/cggraph/canvas : les BACKENDS
# d'ImGui appartiennent a l'HOTE, jamais au canvas, exactement comme dans
# cggraph-boilerplate. C'est le seul endroit ou l'API graphique est choisie.
#
# ⚠ PAS d'imgui_impl_glfw ici, alors que l'hote natif en a un : le port GLFW de
# la chaine Emscripten s'adresse au DOM, et il n'y a pas de DOM dans un Web
# Worker. Les evenements d'entree arrivent par postMessage et sont pousses dans
# ImGuiIO par graph_editor.cpp.
add_executable(maker
    wasm_api.cpp
    graph_api.cpp
    graph_editor.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)

# WebGL 2, et il faut le dire DEUX fois, a deux etages differents :
#  - au backend d'ImGui, dont la detection automatique choisit ES2/WebGL1 sous
#    Emscripten (imgui_impl_opengl3.h, la branche __EMSCRIPTEN__) ;
#  - a la chaine, dont MAX_WEBGL_VERSION vaut 1 par defaut.
# Sans les deux, l'interface serait dessinee en WebGL1 sur un contexte WebGL2
# que le JS de l'apercu a deja cree -- deux contextes sur un canvas, donc un
# refus du navigateur.
target_compile_definitions(maker PRIVATE IMGUI_IMPL_OPENGL_ES3)

# Les avertissements du code tiers ne sont pas les notres, et les melanger aux
# notres rend les notres invisibles. Meme traitement que src/cggraph/canvas, qui
# compile les memes sources : imgui.h construit ses ImDrawList par memset, ce que
# clang signale a chaque unite qui l'inclut.
set_source_files_properties(
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    graph_editor.cpp
    PROPERTIES COMPILE_OPTIONS "-Wno-nontrivial-memcall")

# le module de maillage tire ses deux dependances transitivement (plain-signature link dans les
# CMakeLists des modules natifs).
#
# cggraph_nodes tire cggraph et cgmesh de la meme facon. C'est la premiere fois
# que ces deux cibles sont LIEES par la chaine Emscripten et non seulement
# compilees : le constat E6 du plan (« compilee, jamais liee ») portait sur la
# compilation, et le lien a manque quatre symboles de cgmesh -- Mesh::load,
# Mesh::save, MeshAlgoSmoothingLaplacian::Apply et Mesh_half_edge::simplify --,
# tous ajoutes a la liste EMSCRIPTEN de src/cgmesh/CMakeLists.txt.
#
# cggraph_canvas amene ImGui et imgui-node-editor. C'est la contrepartie chiffree
# de D27 : l'artefact etant unique, les six pages qui n'ouvrent jamais le graphe
# portent le poids du canvas. Le chiffre est mesure avant/apres sur le meme etat
# de l'arbre, jamais estime.
target_link_libraries(maker PRIVATE cggraph_canvas cggraph_ui cggraph_nodes cgmesh)

# Sortie : maker.js + maker.wasm dans maker/web/, ou la page index.html les
# reference en ./maker.js.
set_target_properties(maker PROPERTIES
    OUTPUT_NAME "maker"
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/web")

# Descripteur gothique servi a la page : gothic.html le charge au demarrage comme
# modele par defaut. Il ne peut pas etre lu depuis test/data/ -- le serveur ne sert
# que maker/web/ --, d'ou la copie. La SOURCE reste test/data/, une seule, et la
# copie est un artefact de build comme maker.js a cote.
add_custom_command(TARGET maker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/test/data/gothic-window-instance.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/web/data/gothic-window-instance.json"
    COMMENT "Copie du descripteur gothique par defaut dans maker/web/data/")

# Image servie par defaut aux pages « Image quantification » et « Blocs
# pixelises » : elles ouvrent dessus plutot que sur un viewer vide. Meme
# raison que le descripteur ci-dessus -- le serveur ne sert que maker/web/,
# donc test/data/ n'est pas atteignable depuis le navigateur ; la SOURCE reste
# unique et la copie est un artefact de build.
add_custom_command(TARGET maker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/test/data/jpg/joconde.jpg"
            "${CMAKE_CURRENT_SOURCE_DIR}/web/data/joconde.jpg"
    COMMENT "Copie de l'image par defaut dans maker/web/data/")

# Police servie par defaut a la page « Texte 3D », pour qu'elle ouvre sur un
# resultat plutot que sur un viewer vide -- l'utilisateur reste libre d'importer
# la sienne. BloomingGrove.otf (Nathan Eady) est du DOMAINE PUBLIC, ce qui
# autorise sa redistribution avec la page. Meme montage que les deux copies
# ci-dessus : source unique dans test/data/, copie en artefact de build.
add_custom_command(TARGET maker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/test/data/fonts/BloomingGrove.otf"
            "${CMAKE_CURRENT_SOURCE_DIR}/web/data/BloomingGrove.otf"
    COMMENT "Copie de la police par defaut dans maker/web/data/")

# Exceptions : nanosvg / STL en levent (cf. ANALYSE sec.9.3).
target_compile_options(maker PRIVATE -fexceptions)

# Options Emscripten (cf. ANALYSE sec.9.2) :
#  -lembind            : bindings C++ <-> JS
#  MODULARIZE+EXPORT_ES6 : import ES6 `createMakerModule()` -> Promise<Module>
#  ALLOW_MEMORY_GROWTH : heap extensible (maillages volumineux)
#  FORCE_FILESYSTEM    : MEMFS pour l'import SVG par octets (ANALYSE sec.7)
#  ENVIRONMENT=web,worker : le module doit pouvoir s'instancier AUSSI dans un
#                      Web Worker -- c'est la cible du graphe (option (a), D5 /
#                      D27). Un seul artefact pour les deux : `web` seul ferait
#                      echouer le chargement dans le worker au demarrage.
target_link_options(maker PRIVATE
    -lembind
    -fexceptions
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sEXPORT_NAME=createMakerModule"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sFORCE_FILESYSTEM=1"
    "-sENVIRONMENT=web,worker"
    "-sMAX_WEBGL_VERSION=2"
    "-sEXPORTED_RUNTIME_METHODS=FS,ccall,cwrap")

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
    mesh_payload.cpp
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
    mesh_payload.cpp
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

# GABARITS de pages (template.html). Un gabarit decrit une page entiere -- le
# graphe qu'elle evalue, les parametres qu'elle expose, ses sources de fichier --
# et rien d'autre ne distingue deux pages templatees.
#
# Meme montage que les copies ci-dessus, et pour la meme raison de servabilite :
# le serveur ne sert que maker/web/. Mais il y a ici un motif de PLUS, qui n'existe
# pas pour l'image ni pour les polices -- un gabarit contient un DOCUMENT DE GRAPHE,
# donc quelque chose qui peut etre FAUX : un type de noeud renomme, un port qui a
# change d'indice, un parametre disparu. Le garder sous test/data/ le met a portee
# de la suite de tests, qui le relit et l'evalue reellement
# (tu_cggraph_template.cpp). Une copie servie au navigateur ne se teste pas.
# ⚠ PAS de POST_BUILD ici, contrairement aux trois copies voisines, et la
# difference est de fond : un POST_BUILD ne s'execute que si sa cible est
# RELIEE. Les trois autres copient des ressources qui ne changent qu'avec le
# code ; un gabarit, lui, se modifie SEUL -- c'est meme tout son interet. Sous
# POST_BUILD, editer un gabarit sans toucher a une ligne de C++ ne recopiait
# rien, et le navigateur continuait de servir la version precedente sans que
# rien ne le signale. (Constate, pas suppose.)
#
# D'ou une regle par fichier, qui DEPEND de sa source : elle se redeclenche des
# que le gabarit change, que maker soit relie ou non.
file(GLOB MAKER_TEMPLATES "${CMAKE_SOURCE_DIR}/test/data/templates/*.json")
set(MAKER_TEMPLATE_OUTPUTS "")
foreach(tpl ${MAKER_TEMPLATES})
    get_filename_component(tpl_name "${tpl}" NAME)
    set(tpl_out "${CMAKE_CURRENT_SOURCE_DIR}/web/data/templates/${tpl_name}")
    add_custom_command(OUTPUT "${tpl_out}"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/web/data/templates"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${tpl}" "${tpl_out}"
        DEPENDS "${tpl}"
        COMMENT "Copie du gabarit ${tpl_name} dans maker/web/data/templates/")
    list(APPEND MAKER_TEMPLATE_OUTPUTS "${tpl_out}")
endforeach()
if (MAKER_TEMPLATE_OUTPUTS)
    add_custom_target(maker_templates ALL DEPENDS ${MAKER_TEMPLATE_OUTPUTS})
    add_dependencies(maker maker_templates)
endif()

# Catalogue de polices propose par la page « Texte 3D » : elle ouvre sur la
# police par defaut (catalogue.json / "default") plutot que sur un viewer vide,
# et son selecteur offre les autres -- l'utilisateur reste libre d'importer la
# sienne. Meme montage que les deux copies ci-dessus : source unique dans
# test/data/, copie en artefact de build.
#
# ⚠ Deposer une police dans test/data/fonts/ la REDISTRIBUE : ce glob la sert au
# navigateur. Seules des licences qui l'autorisent (OFL 1.1, Apache 2.0, domaine
# public) ont leur place la, et licenses/ doit porter le texte correspondant --
# l'OFL 1.1 l'exige. Cf. test/data/fonts/CREDITS.md.
#
# GLOB et non liste explicite : ajouter une police ne doit toucher QUE
# test/data/fonts/ (le fichier + son entree dans catalogue.json). Le prix est le
# prix habituel du glob -- il est evalue a la CONFIGURATION, donc une police
# ajoutee sans reconfigurer ne sera pas copiee.
file(GLOB MAKER_FONT_FILES
     "${CMAKE_SOURCE_DIR}/test/data/fonts/*.ttf"
     "${CMAKE_SOURCE_DIR}/test/data/fonts/*.otf"
     "${CMAKE_SOURCE_DIR}/test/data/fonts/*.ttc"
     "${CMAKE_SOURCE_DIR}/test/data/fonts/*.otc")

# copy_if_different accepte plusieurs sources pour un dossier destination, et
# non copy_directory_if_different : celui-la demande CMake 3.26 quand la racine
# n'exige que 3.18.4.
add_custom_command(TARGET maker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/web/data/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MAKER_FONT_FILES}
            "${CMAKE_SOURCE_DIR}/test/data/fonts/catalogue.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/web/data/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/test/data/fonts/licenses"
            "${CMAKE_CURRENT_SOURCE_DIR}/web/data/fonts/licenses"
    COMMENT "Copie du catalogue de polices dans maker/web/data/fonts/")

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

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

add_executable(maker wasm_api.cpp)

# le module de maillage tire ses deux dependances transitivement (plain-signature link dans les
# CMakeLists des modules natifs).
target_link_libraries(maker PRIVATE cgmesh)

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

# Exceptions : nanosvg / STL en levent (cf. ANALYSE sec.9.3).
target_compile_options(maker PRIVATE -fexceptions)

# Options Emscripten (cf. ANALYSE sec.9.2) :
#  -lembind            : bindings C++ <-> JS
#  MODULARIZE+EXPORT_ES6 : import ES6 `createMakerModule()` -> Promise<Module>
#  ALLOW_MEMORY_GROWTH : heap extensible (maillages volumineux)
#  FORCE_FILESYSTEM    : MEMFS pour l'import SVG par octets (ANALYSE sec.7)
#  ENVIRONMENT=web     : cible navigateur uniquement
target_link_options(maker PRIVATE
    -lembind
    -fexceptions
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sEXPORT_NAME=createMakerModule"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sFORCE_FILESYSTEM=1"
    "-sENVIRONMENT=web"
    "-sEXPORTED_RUNTIME_METHODS=FS,ccall,cwrap")

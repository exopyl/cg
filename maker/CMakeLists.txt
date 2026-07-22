# ---------------------------------------------------------------------------
# maker : module WebAssembly (Emscripten) exposant cgmesh au navigateur.
#
# Produit maker.js (glue ES6) + maker.wasm : un pont Embind vers la logique
# geometrique de cgmesh (surfaces parametriques, extrusion SVG, L-systemes).
# La page web de maker/web/ le charge et embarque Online3DViewer comme moteur
# de rendu (cf. maker/ANALYSE.md).
#
# Chaine de dependances : cgmesh -> cgimg -> cgmath (cibles ajoutees par le
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

# cgmesh tire cgimg + cgmath transitivement (plain-signature link dans les
# CMakeLists de cgmesh/cgimg).
target_link_libraries(maker PRIVATE cgmesh)

# Sortie : maker.js + maker.wasm dans maker/web/, ou la page index.html les
# reference en ./maker.js.
set_target_properties(maker PROPERTIES
    OUTPUT_NAME "maker"
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/web")

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

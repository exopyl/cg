# ---------------------------------------------------------------------------
#  FetchRmlUi.cmake -- RmlUi 6.3 et sa seule dependance, FreeType 2.13.3
# ---------------------------------------------------------------------------
#
# ⚠ CE FICHIER TELECHARGE. Il rompt la propriete « configuration hors ligne »
# que le reste du depot tient (extern/ est versionne, wxWidgets et OCCT sont
# pointes par des variables de cache). C'est la consequence directe du choix
# d'amener RmlUi par FetchContent plutot que de le vendoriser. Le preset
# ci-linux ne doit donc pas activer ENABLE_PELES.
#
# ⚠ LES DEUX DEPOTS SONT EPINGLES PAR EMPREINTE DE COMMIT, PAS PAR ETIQUETTE.
# Une etiquette git est MUTABLE : elle peut etre deplacee sur le depot amont,
# et FetchContent ne verifie aucune signature. C'est du code qui entre dans le
# binaire, et le reste du depot tient le standard oppose (extern/ versionne).
# L'etiquette correspondante est conservee en commentaire, pour la lecture
# seulement.
#
# ORDRE CONTRAINT : FreeType D'ABORD, RmlUi ENSUITE. RmlUi appelle
# find_package(Freetype) dans CMake/Dependencies.cmake:10, puis verifie
# l'EXISTENCE DE LA CIBLE `Freetype::Freetype` (CMake/Utilities.cmake:68-71) et
# non le resultat du find_package -- la dependance est declaree « souple »
# precisement pour qu'un projet hote puisse la fournir autrement. C'est cette
# porte que la fonction ci-dessous emprunte.

include(FetchContent)

# 42608f77f20749dd6ddc9e0536788eaad70ea4b5 == etiquette VER-2-13-3
set(PELES_FREETYPE_COMMIT "42608f77f20749dd6ddc9e0536788eaad70ea4b5")
# ba95ffe8bfb6370efb2cdcca927eaad4710c5413 == etiquette 6.3
set(PELES_RMLUI_COMMIT "ba95ffe8bfb6370efb2cdcca927eaad4710c5413")

function(peles_add_rmlui)
    if (TARGET rmlui_core)
        # ⚠ LA VARIABLE DE SORTIE EST RENDUE AVANT DE SORTIR. L'appelant en a
        # besoin pour nommer RmlUi_Renderer_GL2.cpp ; sortir sans la rendre
        # ferait construire le chemin "/RmlUi_Renderer_GL2.cpp" et echouer la
        # configuration sur « Cannot find source file », un diagnostic qui ne
        # nomme ni RmlUi ni cette fonction.
        FetchContent_GetProperties(rmlui SOURCE_DIR _peles_rmlui_source_dir)
        set(PELES_RMLUI_BACKENDS_DIR "${_peles_rmlui_source_dir}/Backends" PARENT_SCOPE)
        return()
    endif ()

    # ⚠ TOUTES LES OPTIONS CI-DESSOUS SONT DES VARIABLES NORMALES DE PORTEE
    # FONCTION, ET AUCUNE N'EST MISE EN CACHE. Les add_subdirectory qu'entraine
    # FetchContent en heritent, et rien n'en sort : une entree de cache FORCEE
    # survivrait a ENABLE_PELES=OFF et deviendrait non modifiable par
    # l'utilisateur, pour des dependances qui ne seraient meme plus baties.
    #
    # Cela suppose CMP0077 en NEW chez les deux dependances, sans quoi leur
    # `option()` ecraserait la variable normale :
    #   - RmlUi declare `cmake_minimum_required(VERSION 3.10...3.27)`, donc elle
    #     y est NEW d'office ;
    #   - FreeType declare `cmake_minimum_required(VERSION 3.0...3.5)`, donc
    #     CMP0077 (introduite en 3.13) y serait NON DEFINIE, c'est-a-dire OLD.
    #     D'ou la ligne ci-dessous, qui n'a pas d'equivalent chez RmlUi.
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

    # BIBLIOTHEQUES STATIQUES PARTOUT. RmlUi declare
    # `option(BUILD_SHARED_LIBS ... ON)` : sans cette ligne, peles.exe exigerait
    # rmlui_core.dll et rmlui_debugger.dll a cote de lui.
    set(BUILD_SHARED_LIBS OFF)

    # -----------------------------------------------------------------------
    #  FreeType
    # -----------------------------------------------------------------------
    #
    # LES CINQ DEPENDANCES OPTIONNELLES SONT COUPEES A LA SOURCE.
    # CMakeLists.txt de FreeType 2.13.3 lignes 253-298 : sans FT_DISABLE_*, il
    # lance find_package sur HarfBuzz, PNG, ZLIB, BZip2 et BrotliDec. Aucune
    # n'est disponible ici. Les find_package non REQUIRED ne font pas echouer la
    # configuration, mais ils resolvent partiellement -- FindPNG tire FindZLIB,
    # et le depot vendorise son propre zlib sous une autre cible -- ce qui
    # melangerait deux zlib dans le meme binaire.
    set(FT_DISABLE_HARFBUZZ ON)
    set(FT_DISABLE_BROTLI   ON)
    set(FT_DISABLE_PNG      ON)
    set(FT_DISABLE_BZIP2    ON)
    set(FT_DISABLE_ZLIB     ON)

    # FreeType 2.13.3 declare `cmake_minimum_required(VERSION 3.0...3.5)` et
    # CMake 4 refuse une borne basse anterieure a 3.5. Meme relevement, meme
    # raison et meme portee que pour GLFW dans cmake/CgVendoredGlfw.cmake : il
    # vit ICI, avec l'appel, pour que la copie amont reste une copie.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

    FetchContent_Declare(freetype
        GIT_REPOSITORY https://github.com/freetype/freetype.git
        GIT_TAG        ${PELES_FREETYPE_COMMIT}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(freetype)

    unset(CMAKE_POLICY_VERSION_MINIMUM)

    # LA CIBLE QUE RMLUI CHERCHE N'EXISTE PAS DANS L'ARBRE DE CONSTRUCTION DE
    # FREETYPE. CMakeLists.txt de FreeType 2.13.3 :456 cree la cible `freetype` ;
    # :516-518 cree une interface `freetype-interface` dont l'EXPORT_NAME vaut
    # `Freetype::Freetype`. Un EXPORT_NAME ne produit un nom qu'a l'INSTALLATION :
    # rien ne repond a `Freetype::Freetype` avant. D'ou l'alias, pose ici.
    if (NOT TARGET Freetype::Freetype)
        add_library(Freetype::Freetype ALIAS freetype)
    endif ()

    # -----------------------------------------------------------------------
    #  RmlUi
    # -----------------------------------------------------------------------
    #
    # Les echantillons tirent un backend complet (fenetre, boucle, renderer) et
    # leurs propres assets : rien de tout cela n'est utilise ici, et
    # RMLUI_SAMPLES=OFF laisse aussi RMLUI_SHELL a OFF, donc Backends/ n'est pas
    # ajoute comme sous-repertoire. Le SEUL fichier de Backends/ que peles
    # compile est RmlUi_Renderer_GL2.cpp, et il le compile lui-meme -- c'est
    # l'usage que RmlUi prescrit pour ses *renderers*, par opposition a ses
    # *backends*.
    set(RMLUI_SAMPLES OFF)
    set(RMLUI_LUA_BINDINGS OFF)
    set(RMLUI_LOTTIE_PLUGIN OFF)
    set(RMLUI_SVG_PLUGIN OFF)
    # ⚠ RMLUI_FONT_ENGINE n'est pas une `option()` mais un `set(... CACHE
    # STRING)` (CMakeLists.txt:35), que CMP0077 ne couvre pas. La variable
    # normale le shadowe dans cette portee ; si la politique CMP0126 du
    # sous-projet la retirait, la valeur retenue serait la valeur par defaut
    # amont -- qui est deja "freetype". Dans les deux cas le resultat est le
    # meme, et dans aucun l'utilisateur ne se retrouve avec une entree forcee.
    set(RMLUI_FONT_ENGINE "freetype")

    FetchContent_Declare(rmlui
        GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
        GIT_TAG        ${PELES_RMLUI_COMMIT}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(rmlui)

    # Le repertoire des renderers, remonte a l'appelant : peles_platform y prend
    # RmlUi_Renderer_GL2.{h,cpp}. Il n'existe aucune cible pour ce fichier, donc
    # aucun autre moyen de le nommer.
    set(PELES_RMLUI_BACKENDS_DIR "${rmlui_SOURCE_DIR}/Backends" PARENT_SCOPE)
endfunction()

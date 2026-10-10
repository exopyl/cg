# ---------------------------------------------------------------------------
#  GLFW versionne : un seul point d'ajout, pour tous les hotes natifs du depot.
# ---------------------------------------------------------------------------
#
# GLFW est versionne sous extern/glfw-<version> (licence zlib/libpng). N'en sont
# embarques que le projet CMake, include/ et src/ : ni exemples, ni tests, ni
# documentation. Une seule version dans l'arbre -- deux seraient une source de
# confusion sans contrepartie.
#
# LA FONCTION EST IDEMPOTENTE, et c'est sa raison d'etre. Plusieurs options
# independantes tirent la meme fenetre (ENABLE_CGGRAPH_BOILERPLATE, ENABLE_PELES) ;
# elles doivent etre combinables DANS N'IMPORTE QUEL ORDRE, or un second
# add_subdirectory sur le meme arbre redefinirait la cible `glfw` et ferait
# echouer la configuration. Le garde est la presence de la cible, pas un drapeau
# maison : c'est exactement la condition qui casse.

function(cg_add_vendored_glfw)
    if (TARGET glfw)
        return()
    endif ()

    # DEUX PANNES DISTINCTES, DEUX MESSAGES. Une variable absente et un chemin
    # faux ne se corrigent pas au meme endroit : la premiere se pose dans le
    # CMakeLists.txt appelant, le second se corrige dans le cache. Un message
    # unique affichant une chaine vide les confondrait.
    if (NOT DEFINED GLFW_DIR OR GLFW_DIR STREQUAL "")
        message(FATAL_ERROR
            "GLFW_DIR n'est pas definie : cg_add_vendored_glfw() ne sait pas quel arbre "
            "ajouter. Elle est posee par le CMakeLists.txt racine de ce depot ; un projet "
            "qui appelle cette fonction sans l'inclure doit la definir lui-meme.")
    endif ()

    if (NOT EXISTS "${GLFW_DIR}")
        message(FATAL_ERROR "GLFW_DIR pointe un chemin inexistant : ${GLFW_DIR}")
    endif ()

    # Les quatre extras d'amont, tous eteints : ils ajoutent des cibles et des
    # regles d'installation dont aucun consommateur de ce depot n'a l'usage, et
    # GLFW_INSTALL ferait entrer glfw dans le `install()` du projet hote.
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

    # Le CMakeLists amont de GLFW 3.3.10 declare `cmake_minimum_required(3.0...3.20)`
    # et CMake 4 refuse une borne basse anterieure a 3.5. La borne est relevee ICI,
    # pour ce sous-arbre seulement : editer le fichier amont ferait de la copie
    # versionnee un fork a re-appliquer a chaque montee de version.
    #
    # LA RAISON VOYAGE AVEC L'APPEL. Sans ce relevement, la configuration echoue
    # sous CMake 4 sur un message qui ne nomme pas GLFW.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

    # Repertoire de construction FIXE, et non ${CMAKE_CURRENT_BINARY_DIR} : la
    # fonction peut etre appelee depuis n'importe quel sous-repertoire, et c'est
    # le premier appel qui gagne. Un chemin derive de l'appelant ferait dependre
    # l'emplacement des artefacts de l'ordre des options.
    add_subdirectory(${GLFW_DIR} ${CMAKE_BINARY_DIR}/extern/glfw EXCLUDE_FROM_ALL)

    unset(CMAKE_POLICY_VERSION_MINIMUM)
endfunction()

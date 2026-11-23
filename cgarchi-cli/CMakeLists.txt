cmake_minimum_required(VERSION 3.18.4)

project(cgarchi-cli)

# Ligne de commande des colonnes classiques (cgarchi) : reglages
# "classical-column/v1" en entree, maillage OBJ / GLB / STL et rapport JSON en
# sortie. Le serveur MCP de mcp/ l'appelle.
add_executable(cgarchi-cli main.cpp)
target_link_libraries(cgarchi-cli PRIVATE cgarchi)

# Bibliotheques que cgmesh importe : copiees a cote de l'exe pour qu'il demarre
# hors de l'arbre de build (memes blocs que test/CMakeLists.txt).
if (WIN32 AND ENABLE_OPENNURBS)
    add_custom_command(TARGET cgarchi-cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_OPENNURBS}/lib/vc14x_x64/OpenNURBS.dll"
            "$<TARGET_FILE_DIR:cgarchi-cli>")
endif()
if (WIN32 AND ENABLE_LIB3MF)
    add_custom_command(TARGET cgarchi-cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_LIB3MF}/bin/lib3mf.dll"
            "$<TARGET_FILE_DIR:cgarchi-cli>")
endif()
if (WIN32 AND ENABLE_OCCT)
    add_custom_command(TARGET cgarchi-cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E ${CG_COPY_DIRECTORY}
            "${EXTERN_OCCT}/bin"
            "$<TARGET_FILE_DIR:cgarchi-cli>")
endif()

cmake_minimum_required(VERSION 3.18.4)

project(libcgimg)

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)

add_library(cgimg STATIC ${HEADER_FILES} ${SRC_FILES})

# extern/ provides stb_image.h (used by image_io_png.cpp for PNG decoding).
target_include_directories(cgimg PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../extern)

# Décodeurs PNG / JPEG (image_io_png.cpp / image_io_jpg.cpp), gardés par
# #ifdef CGIMG_WITH_PNG / #ifdef CGIMG_WITH_JPG. Désactivés, Img::load renvoie -1
# pour un .png/.jpg : les textures (map_Kd) ne se chargent pas et le modèle
# s'affiche gris. Les deux décodeurs s'appuient sur stb_image.h (extern/) en
# STB_IMAGE_STATIC, donc pas de conflit de symboles à l'édition de liens.
#
# PRIVATE, et non PUBLIC : les seules déclarations gardées par ces macros sont
# celles d'ImgIO (image_io.h), en-tête inclus uniquement par les .cpp de cgimg.
# Aucun consommateur ne voit ces macros ni ces déclarations — c'est précisément
# l'objectif, les anciens noms PNG / JPG étant #define dans image.h (en-tête
# public) et fuyant dans chaque unité de compilation incluant cgimg.
#
# Les options portent le même nom que les macros : ce sont bien elles qui
# pilotent la fonctionnalité, contrairement à l'ancien montage où le #define de
# image.h rendait la définition CMake inerte.
option(CGIMG_WITH_PNG "cgimg : import PNG via stb_image" ON)
option(CGIMG_WITH_JPG "cgimg : import JPEG via stb_image" ON)

if (CGIMG_WITH_PNG)
	target_compile_definitions(cgimg PRIVATE CGIMG_WITH_PNG)
endif ()
if (CGIMG_WITH_JPG)
	target_compile_definitions(cgimg PRIVATE CGIMG_WITH_JPG)
endif ()

target_link_libraries(cgimg cgmath)

install(TARGETS cgimg DESTINATION .)

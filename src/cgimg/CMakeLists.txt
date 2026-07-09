cmake_minimum_required(VERSION 3.18.4)

project(libcgimg)

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)

add_library(cgimg STATIC ${HEADER_FILES} ${SRC_FILES})

# extern/ provides stb_image.h (used by image_io_png.cpp for PNG decoding).
target_include_directories(cgimg PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../extern)

# Active les décodeurs PNG/JPEG (image_io_png.cpp / image_io_jpg.cpp) : ils sont
# gardés par #ifdef PNG / #ifdef JPG. Sans ces définitions, Img::load renvoie -1
# pour un .png/.jpg, les textures (map_Kd) ne se chargent pas et le modèle
# s'affiche gris. Les deux décodeurs s'appuient sur stb_image.h (extern/) en
# STB_IMAGE_STATIC, donc pas de conflit de symboles à l'édition de liens.
target_compile_definitions(cgimg PRIVATE PNG JPG)

target_link_libraries(cgimg cgmath)

install(TARGETS cgimg DESTINATION .)

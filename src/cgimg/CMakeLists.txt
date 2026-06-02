cmake_minimum_required(VERSION 3.18.4)

project(libcgimg)

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)

add_library(cgimg STATIC ${HEADER_FILES} ${SRC_FILES})

# extern/ provides stb_image.h (used by image_io_png.cpp for PNG decoding).
target_include_directories(cgimg PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../extern)

target_link_libraries(cgimg cgmath)

install(TARGETS cgimg DESTINATION .)

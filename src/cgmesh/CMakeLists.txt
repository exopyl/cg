cmake_minimum_required(VERSION 3.18.4)

project(libcgmesh)

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)

add_compile_options(-fpermissive)

add_library(cgmesh SHARED ${HEADER_FILES} ${SRC_FILES})

install(TARGETS cgmesh DESTINATION .)

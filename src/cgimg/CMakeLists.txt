cmake_minimum_required(VERSION 3.18.4)

project(libcgimg)

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)

add_library(cgimg SHARED ${HEADER_FILES} ${SRC_FILES})

install(TARGETS cgimg DESTINATION .)

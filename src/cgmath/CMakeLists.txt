cmake_minimum_required(VERSION 3.18.4)

project(libcgmath)

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)

add_library(cgmath STATIC ${HEADER_FILES} ${SRC_FILES})

# Expose third-party headers under src/extern (e.g. nlohmann/json.hpp) so that
# cgmath sources and consumers can use angle-bracket includes like
#   #include <nlohmann/json.hpp>
target_include_directories(cgmath PUBLIC ${CMAKE_SOURCE_DIR}/extern)

install(TARGETS cgmath DESTINATION .)

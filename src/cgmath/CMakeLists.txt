cmake_minimum_required(VERSION 3.18.4)

project(libcgmath)

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)

add_library(cgmath STATIC ${HEADER_FILES} ${SRC_FILES})

# Expose third-party headers under <repo>/extern (e.g. nlohmann/json.hpp)
# so that cgmath sources and consumers can use angle-bracket includes like
#   #include <nlohmann/json.hpp>
# Use a path relative to cgmath's own location so this works whether cgmath
# is built from the repo-root CMakeLists or as a subproject of vecna/.
target_include_directories(cgmath PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../extern)

install(TARGETS cgmath DESTINATION .)

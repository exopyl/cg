cmake_minimum_required(VERSION 3.18.4)

project(libcgmath)

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)

add_library(cgmath STATIC ${HEADER_FILES} ${SRC_FILES})

# Third-party headers under <repo>/extern (e.g. nlohmann/json.hpp), used only
# by cgmath's own .cpp (architecture_gothic_io.cpp) -> PRIVATE so the dependency
# does not leak to consumers. Path relative to cgmath's own location so this
# works whether built from the repo-root CMakeLists or as a subproject of vecna/.
target_include_directories(cgmath PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../extern)

install(TARGETS cgmath DESTINATION .)

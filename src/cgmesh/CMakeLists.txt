cmake_minimum_required(VERSION 3.18.4)

project(libcgmesh)

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)

if (UNIX)
	add_compile_options(-fpermissive)
endif (UNIX)

add_library(cgmesh STATIC ${HEADER_FILES} ${SRC_FILES})
target_link_libraries(cgmesh cgimg)

install(TARGETS cgmesh DESTINATION .)

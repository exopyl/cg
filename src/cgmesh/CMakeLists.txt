cmake_minimum_required(VERSION 3.18.4)

project(libcgmesh)

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)
file(GLOB GLUTESS_SRC_FILES ${CMAKE_SOURCE_DIR}/extern/glutess/*.c)
file(GLOB GLUTESS_HEADER_FILES ${CMAKE_SOURCE_DIR}/extern/glutess/*.h)

if (UNIX)
	add_compile_options(-fpermissive)
endif (UNIX)

add_library(cgmesh STATIC ${HEADER_FILES} ${SRC_FILES} ${GLUTESS_HEADER_FILES} ${GLUTESS_SRC_FILES})
source_group("glutess" FILES ${GLUTESS_HEADER_FILES} ${GLUTESS_SRC_FILES})
target_include_directories(cgmesh PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(cgmesh cgimg)

install(TARGETS cgmesh DESTINATION .)

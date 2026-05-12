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

if (ENABLE_OPENNURBS)
    if (NOT EXTERN_OPENNURBS)
        message(FATAL_ERROR "ENABLE_OPENNURBS is ON but EXTERN_OPENNURBS is not set.")
    endif()
    target_compile_definitions(cgmesh PRIVATE CG_HAS_OPENNURBS OPENNURBS_IMPORTS)
    target_include_directories(cgmesh PRIVATE ${EXTERN_OPENNURBS}/include)
    if (WIN32)
        target_link_libraries(cgmesh ${EXTERN_OPENNURBS}/lib/vc14x_x64/OpenNURBS.lib)
    endif()
endif()

install(TARGETS cgmesh DESTINATION .)

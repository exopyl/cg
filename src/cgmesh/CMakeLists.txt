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
        set(EXTERN_OPENNURBS "${CMAKE_SOURCE_DIR}/extern/opennurbs-v8.27.26019.16021")
    endif()
    
    if (NOT EXISTS "${EXTERN_OPENNURBS}/include")
        message(FATAL_ERROR "ENABLE_OPENNURBS is ON but EXTERN_OPENNURBS/include does not exist at ${EXTERN_OPENNURBS}/include")
    endif()

    target_compile_definitions(cgmesh PUBLIC CG_HAS_OPENNURBS)
    target_compile_definitions(cgmesh PRIVATE OPENNURBS_IMPORTS)
    target_include_directories(cgmesh PUBLIC ${EXTERN_OPENNURBS}/include)

    if (WIN32)
        target_link_libraries(cgmesh ${EXTERN_OPENNURBS}/lib/vc14x_x64/OpenNURBS.lib)
    elseif (UNIX)
        target_link_libraries(cgmesh ${EXTERN_OPENNURBS}/lib/linux_x64/libOpenNURBS.so)
    endif()
endif()

install(TARGETS cgmesh DESTINATION .)

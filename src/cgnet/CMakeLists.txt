cmake_minimum_required(VERSION 3.18.4)

project(libcgnet)

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)

add_library(cgnet STATIC ${HEADER_FILES} ${SRC_FILES})

target_include_directories(cgnet PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/extern/cpp-httplib
)

if (WIN32)
    target_link_libraries(cgnet ws2_32 Iphlpapi Mpr)
endif()

install(TARGETS cgnet DESTINATION .)

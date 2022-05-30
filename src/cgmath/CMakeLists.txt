cmake_minimum_required(VERSION 3.18.4)

project(libcgmath)

set(srcs
    common.cpp
    vec3f.cpp
    )
    
set(headers
    common.h
    vec3f.h
    )

add_library(cgmath SHARED ${headers} ${srcs})

install(TARGETS cgmath DESTINATION .)

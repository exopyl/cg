cmake_minimum_required (VERSION 3.18.4)
project (cg)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -g")
set(CMAKE_INSTALL_PREFIX "./build")

#set(build "build")
#set(include_dest "include/${PROJECT_NAME}")

add_subdirectory(src)
add_subdirectory(test)

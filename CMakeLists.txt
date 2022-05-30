cmake_minimum_required (VERSION 3.18.4)
project (cg)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -g")
set(CMAKE_INSTALL_PREFIX "./build")

enable_testing()

add_subdirectory(src)
add_subdirectory(test)

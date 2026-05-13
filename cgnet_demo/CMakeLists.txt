cmake_minimum_required(VERSION 3.18.4)

project(cgnet_demo)

add_executable(cgnet_demo main.cpp)

target_link_libraries(cgnet_demo cgnet)

set_target_properties(cgnet_demo PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:cgnet_demo>"
)

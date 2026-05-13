cmake_minimum_required(VERSION 3.18.4)
include(GoogleTest)

project(TU)

# Options
if ( ENABLE_COVERAGE )
	set(CMAKE_CXX_FLAGS "-g -O0 -Wall -W -Wshadow -Wunused-variable -Wunused-parameter -Wunused-function -Wunused -Wno-system-headers -Wno-deprecated -Woverloaded-virtual -Wwrite-strings -Wno-pointer-to-int-cast -fprofile-instr-generate -fcoverage-mapping")
endif ()

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)

add_executable(${PROJECT_NAME} ${HEADER_FILES} ${SRC_FILES})

# Pour Visual Studio : generer les fichiers de sortie dans le repertoire de build avec suffixe de config
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)

# Copier test/data dans le repertoire de l'executable de test apres build
if(EXISTS "${CMAKE_SOURCE_DIR}/test/data")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/test/data"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/test/data"
        COMMENT "Copying test/data to $<TARGET_FILE_DIR:${PROJECT_NAME}>/test/data"
    )
endif()

if (UNIX AND ENABLE_OPENNURBS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_OPENNURBS}/lib/linux_x64/libOpenNURBS.so"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying libOpenNURBS.so to $<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()

if (WIN32 AND ENABLE_OPENNURBS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_OPENNURBS}/lib/vc14x_x64/OpenNURBS.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying OpenNURBS.dll to $<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()

if (WIN32 AND EXTERN_GOOGLETEST)
    add_subdirectory(${EXTERN_GOOGLETEST} gtest)
    target_link_libraries(${PROJECT_NAME} GTest::gtest_main)
    target_link_libraries(${PROJECT_NAME} GTest::gmock_main)
elseif (NOT WIN32)
    find_package(GTest REQUIRED)
    target_link_libraries(${PROJECT_NAME} GTest::gtest_main)
endif (WIN32 AND EXTERN_GOOGLETEST)

gtest_discover_tests(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} cgmath cgimg cgmesh)
if ( WIN32 )
    target_link_libraries(${PROJECT_NAME} cgnet)
endif ()
if ( ENABLE_COVERAGE )
    if ( UNIX )
        target_link_libraries(${PROJECT_NAME} pthread gcov)
    endif ()
endif ()

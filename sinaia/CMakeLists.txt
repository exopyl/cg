cmake_minimum_required(VERSION 3.20)
project(sinaia)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_VERBOSE_MAKEFILE 1)
set(APP_VERSION_MAJOR 1)
set(APP_VERSION_MINOR 0)

set(APP_TARGET sinaia)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(../src/cgre)

set(srcs
        DrawingArea.cpp
        Managers.cpp
        PropertyPanel.cpp
        SettingsPanel.cpp
        SinaiaApp.cpp
        SinaiaFrame.cpp
        wxOpenGLCanvas.cpp
        wxSizeReportCtrl.cpp
        )

add_executable(${APP_TARGET} WIN32 ${srcs})
target_link_libraries(${APP_TARGET} cgimg cgmath cgmesh cgre)

# opengl
find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIRS})

# wxWidgets
set(wxWidgets_ROOT_DIR ${EXTERN_WXWIDGETS})

find_package(wxWidgets REQUIRED COMPONENTS core base aui html gl propgrid)
if(wxWidgets_USE_FILE) # not defined in CONFIG mode
    include(${wxWidgets_USE_FILE})
endif()
target_link_libraries(${APP_TARGET} ${wxWidgets_LIBRARIES})
target_include_directories(${APP_TARGET} PRIVATE ${wxWidgets_INCLUDE_DIRS})

target_link_libraries(
        ${APP_TARGET}
        ${OPENGL_LIBRARIES}
)

# Copy wxWidgets DLLs next to the executable
if(WIN32)
    # Use the discovered library directory for DLLs.
    # The names are constructed to include 'd' for Debug config if needed.
    set(WX_DLL_INSTALL_DIR "${wxWidgets_LIB_DIR}") 

    set(WX_DLL_NAMES
            wxmsw332u$<IF:$<CONFIG:Debug>,d,>_core_vc14x_x64
            wxbase332u$<IF:$<CONFIG:Debug>,d,>_vc14x_x64
            wxmsw332u$<IF:$<CONFIG:Debug>,d,>_aui_vc14x_x64
            wxmsw332u$<IF:$<CONFIG:Debug>,d,>_html_vc14x_x64
            wxmsw332u$<IF:$<CONFIG:Debug>,d,>_gl_vc14x_x64
            wxmsw332u$<IF:$<CONFIG:Debug>,d,>_propgrid_vc14x_x64)
    foreach(DLL_NAME ${WX_DLL_NAMES})
        add_custom_command(TARGET ${APP_TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${WX_DLL_INSTALL_DIR}/${DLL_NAME}.dll"
                "$<TARGET_FILE_DIR:${APP_TARGET}>")
        endforeach()

    if (ENABLE_OPENNURBS)
        add_custom_command(TARGET ${APP_TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${EXTERN_OPENNURBS}/lib/vc14x_x64/OpenNURBS.dll"
                "$<TARGET_FILE_DIR:${APP_TARGET}>")
    endif()
endif()
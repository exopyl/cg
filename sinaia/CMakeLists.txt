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

target_link_libraries(
        ${APP_TARGET}
        ${OPENGL_LIBRARIES}
)

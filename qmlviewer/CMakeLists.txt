cmake_minimum_required(VERSION 3.21)

project(qmlviewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick)

# cgre2 is added by the root CMakeLists.txt when ENABLE_QMLVIEWER is ON;
# its DeviceContext.cpp + Pipeline / Buffer / Shader / Trackball / Logger
# implementations all live in the `cgre2` static lib. Vulkan needs to be
# findable at link time.
find_package(Vulkan REQUIRED)

# Shader compilation (vecna's basic.vert/frag, reused as-is for the
# cgre2-under-QML path in CgreQuickItem). glslc comes from VulkanSDK.
find_program(GLSLC glslc
    HINTS $ENV{VULKAN_SDK}/Bin $ENV{VULKAN_SDK}/bin
)
if(NOT GLSLC)
    message(FATAL_ERROR "qmlviewer: glslc not found — install Vulkan SDK or set VULKAN_SDK")
endif()

set(QMLVIEWER_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${QMLVIEWER_SHADER_DIR})

set(QMLVIEWER_SHADER_SOURCES
    ${CMAKE_SOURCE_DIR}/vecna/shaders/basic.vert
    ${CMAKE_SOURCE_DIR}/vecna/shaders/basic.frag
)

set(_qmlviewer_spirv "")
foreach(_shader ${QMLVIEWER_SHADER_SOURCES})
    get_filename_component(_shader_name ${_shader} NAME)
    set(_spv_out "${QMLVIEWER_SHADER_DIR}/${_shader_name}.spv")
    add_custom_command(
        OUTPUT ${_spv_out}
        COMMAND ${GLSLC} -o ${_spv_out} ${_shader}
        DEPENDS ${_shader}
        COMMENT "qmlviewer shaders: ${_shader_name} -> .spv"
        VERBATIM
    )
    list(APPEND _qmlviewer_spirv ${_spv_out})
endforeach()
add_custom_target(qmlviewer_shaders ALL DEPENDS ${_qmlviewer_spirv})

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(qmlviewer
    src/main.cpp
)

qt_add_qml_module(qmlviewer
    URI QmlViewer
    VERSION 1.0
    QML_FILES
        qml/Main.qml
    SOURCES
        src/MeshModel.h
        src/MeshModel.cpp
        src/QtDeviceAdapter.h
        src/QtDeviceAdapter.cpp
        src/CgreQuickItem.h
        src/CgreQuickItem.cpp
)

# Sample meshes live under <repo>/test/data/. Pass the absolute path as a
# compile definition so QML can locate presets without relying on the CWD.
target_compile_definitions(qmlviewer PRIVATE
    QMLVIEWER_DATA_DIR="${CMAKE_SOURCE_DIR}/test/data"
    QMLVIEWER_SHADER_DIR="${QMLVIEWER_SHADER_DIR}"
)

add_dependencies(qmlviewer qmlviewer_shaders)

# Auto-generated qmltyperegistrations.cpp includes "<MeshModel.h>" — needs
# src/ on the include path to locate the header.
target_include_directories(qmlviewer PRIVATE src)

target_link_libraries(qmlviewer
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Vulkan::Vulkan
        cgre2
        cgmesh
)

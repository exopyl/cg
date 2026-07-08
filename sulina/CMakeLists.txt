cmake_minimum_required(VERSION 3.21)

project(sulina LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick Network)

# cgre2 is added by the root CMakeLists.txt when ENABLE_SULINA is ON;
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
    message(FATAL_ERROR "sulina: glslc not found — install Vulkan SDK or set VULKAN_SDK")
endif()

set(SULINA_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SULINA_SHADER_DIR})

set(SULINA_SHADER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/textured.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/textured.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/unlit.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/unlit.frag
)

set(_sulina_spirv "")
foreach(_shader ${SULINA_SHADER_SOURCES})
    get_filename_component(_shader_name ${_shader} NAME)
    set(_spv_out "${SULINA_SHADER_DIR}/${_shader_name}.spv")
    add_custom_command(
        OUTPUT ${_spv_out}
        COMMAND ${GLSLC} -o ${_spv_out} ${_shader}
        DEPENDS ${_shader}
        COMMENT "sulina shaders: ${_shader_name} -> .spv"
        VERBATIM
    )
    list(APPEND _sulina_spirv ${_spv_out})
endforeach()
add_custom_target(sulina_shaders ALL DEPENDS ${_sulina_spirv})

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(sulina
    src/main.cpp
    src/WindowController.h
    src/TcpLogServer.h
    src/TcpLogServer.cpp
    src/ControlServer.h
    src/ControlServer.cpp
)

# Theme.qml is a QML singleton (design tokens + tool metadata) — flag it
# so `Theme.x` resolves without an explicit instance.
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(sulina
    URI Sulina
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/AppIcon.qml
        qml/GlassPanel.qml
        qml/AnalysisDock.qml
        qml/AnalysisPanel.qml
        qml/LegendCard.qml
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
target_compile_definitions(sulina PRIVATE
    SULINA_DATA_DIR="${CMAKE_SOURCE_DIR}/test/data"
    SULINA_SHADER_DIR="${SULINA_SHADER_DIR}"
)

add_dependencies(sulina sulina_shaders)

# Auto-generated qmltyperegistrations.cpp includes "<MeshModel.h>" — needs
# src/ on the include path to locate the header.
target_include_directories(sulina PRIVATE src)

target_link_libraries(sulina
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Network
        Vulkan::Vulkan
        cgre2
        cgmesh
)

# --- Windows: auto-deploy the Qt runtime next to the .exe ----------------
# windeployqt copies the Qt DLLs, QML modules and platform plugins into the
# build output dir, so sulina.exe runs without a manual deploy step or Qt
# on PATH. Runs after every build of the target (windeployqt is incremental);
# the config switch keeps debug/release runtimes matched. Qt6::windeployqt is
# the imported tool target — no hard-coded path.
if (WIN32 AND TARGET Qt6::windeployqt)
    add_custom_command(TARGET sulina POST_BUILD
        COMMAND Qt6::windeployqt
                --no-translations
                $<IF:$<CONFIG:Debug>,--debug,--release>
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                "$<TARGET_FILE:sulina>"
        COMMENT "windeployqt: deploying Qt runtime next to sulina.exe"
        VERBATIM)
endif()

# --- Redistributable bundle: `cmake --build . --config Release --target dist`
# Stages a clean, self-contained copy of the app into <repo>/dist/sulina:
# the exe + the windeployqt'd Qt runtime, minus the dev-only artifacts
# (.lib/.exp/.pdb), plus the MSVC runtime DLLs so it runs without vc_redist.
# The result is ready to zip and ship. Build it in Release for distribution.
if (WIN32)
    set(SULINA_DIST_DIR "${CMAKE_SOURCE_DIR}/dist/sulina")

    # Populate CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS (vcruntime140.dll, msvcp140.dll
    # …) without registering install() rules — we copy them ourselves below.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP ON)
    include(InstallRequiredSystemLibraries)

    add_custom_target(dist
        COMMAND ${CMAKE_COMMAND}
                -DSRC_DIR=$<TARGET_FILE_DIR:sulina>
                -DDIST_DIR=${SULINA_DIST_DIR}
                "-DMSVC_RUNTIME_LIBS=$<JOIN:${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS},$<SEMICOLON>>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MakeDist.cmake"
        COMMENT "dist: staging redistributable bundle into ${SULINA_DIST_DIR}"
        VERBATIM)
    # Ensure the app (and its windeployqt POST_BUILD step) is built first.
    add_dependencies(dist sulina)
endif()

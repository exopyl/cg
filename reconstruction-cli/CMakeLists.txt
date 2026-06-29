cmake_minimum_required(VERSION 3.18.4)

project(reconstruction-cli)

# Pilote de la chaîne de reconstruction (recon::Pipeline). La couche ONNX vit dans
# cgmesh (ENABLE_ONNX) ; ce demo ne dépend que de cgmesh (ORT lié transitivement).
if (NOT ENABLE_ONNX)
    message(FATAL_ERROR "reconstruction-cli requires ENABLE_ONNX=On.")
endif()

add_executable(reconstruction-cli main.cpp)

target_include_directories(reconstruction-cli PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(reconstruction-cli cgmesh cgimg cgmath)

# onnxruntime.dll à côté de l'exe (cgmesh importe ORT sous ENABLE_ONNX).
if (WIN32)
    add_custom_command(TARGET reconstruction-cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_ONNXRUNTIME}/lib/onnxruntime.dll"
            "$<TARGET_FILE_DIR:reconstruction-cli>")
endif()

set_target_properties(reconstruction-cli PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:reconstruction-cli>")

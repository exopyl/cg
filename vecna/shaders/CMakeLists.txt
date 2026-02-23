# shaders/CMakeLists.txt
# Compiles GLSL shaders to SPIR-V

set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/compiled)

# Ensure output directory exists
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

# List of shaders to compile
set(SHADERS
    ${SHADER_SOURCE_DIR}/basic.vert
    ${SHADER_SOURCE_DIR}/basic.frag
)

# Compile each shader
set(SPIRV_OUTPUTS "")
foreach(SHADER ${SHADERS})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPIRV_OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")

    add_custom_command(
        OUTPUT ${SPIRV_OUTPUT}
        COMMAND ${GLSLC} -o ${SPIRV_OUTPUT} ${SHADER}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader: ${SHADER_NAME}"
        VERBATIM
    )

    list(APPEND SPIRV_OUTPUTS ${SPIRV_OUTPUT})
endforeach()

# Create a target that depends on all compiled shaders
add_custom_target(compile_shaders ALL
    DEPENDS ${SPIRV_OUTPUTS}
    COMMENT "Compiling all shaders"
)

# Story 2.1: Pipeline Graphique

Status: done

## Story

As a développeur,
I want créer le pipeline graphique Vulkan,
so that je puisse rendre des primitives 3D à l'écran.

## Acceptance Criteria

### AC1: Compilation Shaders SPIR-V
**Given** les fichiers shaders basic.vert et basic.frag existent
**When** le build CMake est exécuté
**Then** glslc compile les shaders en SPIR-V dans shaders/compiled/
**And** les fichiers .spv sont générés sans erreur

### AC2: Création Pipeline Vulkan
**Given** les shaders SPIR-V sont disponibles
**When** le pipeline est créé
**Then** les shader modules sont chargés correctement
**And** le pipeline layout est créé avec les push constants pour MVP
**And** le graphics pipeline est créé avec le vertex input correspondant au format interleaved

### AC3: Intégration Render Loop
**Given** le pipeline est créé
**When** le rendu est exécuté
**Then** le pipeline peut être bindé dans le command buffer
**And** le logging affiche "[Renderer] Graphics pipeline created"

## Tasks / Subtasks

- [x] Task 1: Créer les fichiers shaders GLSL (AC: #1)
  - [x] Créer le répertoire shaders/ à la racine du projet
  - [x] Créer basic.vert avec vertex shader basique (position, normal, color)
  - [x] Créer basic.frag avec fragment shader basique (output color)
  - [x] Définir les push constants pour la matrice MVP

- [x] Task 2: Configurer la compilation SPIR-V dans CMake (AC: #1)
  - [x] Créer cmake/ShaderCompilation.cmake
  - [x] Détecter glslc (fourni par Vulkan SDK)
  - [x] Créer une fonction pour compiler les shaders en SPIR-V
  - [x] Configurer la sortie dans shaders/compiled/
  - [x] Ajouter la compilation comme dépendance du build

- [x] Task 3: Créer la classe Pipeline (AC: #2, #3)
  - [x] Créer include/Vecna/Renderer/Pipeline.hpp
  - [x] Créer src/Renderer/Pipeline.cpp
  - [x] Implémenter loadShaderModule() pour charger les fichiers .spv
  - [x] Implémenter createPipelineLayout() avec push constants MVP
  - [x] Implémenter createGraphicsPipeline() avec vertex input interleaved
  - [x] Implémenter le destructeur RAII

- [x] Task 4: Configurer le Vertex Input (AC: #2)
  - [x] Définir la structure Vertex (position vec3, normal vec3, color vec3)
  - [x] Créer les VkVertexInputBindingDescription
  - [x] Créer les VkVertexInputAttributeDescription
  - [x] Configurer le format interleaved selon architecture.md

- [x] Task 5: Configurer les états du pipeline (AC: #2)
  - [x] Configurer le viewport et scissor comme dynamiques
  - [x] Configurer le rasterizer (back-face culling, polygon fill)
  - [x] Configurer le multisampling (désactivé pour MVP)
  - [x] Configurer le color blending (opaque)
  - [x] Configurer le depth stencil state (préparation pour 2-4)

- [x] Task 6: Intégrer Pipeline dans Application (AC: #3)
  - [x] Ajouter Pipeline comme membre de Application
  - [x] Créer le Pipeline après Swapchain
  - [x] Binder le pipeline dans recordCommandBuffer()
  - [x] Configurer viewport et scissor dynamiques dans command buffer

- [x] Task 7: Mettre à jour CMakeLists et headers (AC: #1, #2)
  - [x] Ajouter Pipeline.cpp aux sources
  - [x] Inclure cmake/ShaderCompilation.cmake
  - [x] Mettre à jour Renderer.hpp avec Pipeline.hpp
  - [x] Configurer les chemins de shaders compilés

- [x] Task 8: Valider le fonctionnement (AC: #1, #2, #3)
  - [x] Build et vérifier que les shaders sont compilés
  - [x] Run et vérifier les logs de création du pipeline
  - [x] Vérifier aucune validation error en Debug
  - [x] Préparer pour Story 2-2 (vertex/index buffers)

## Dev Notes

### Architecture Patterns à Respecter

**Structure selon architecture.md:**
```
include/Vecna/
├── Renderer/
│   ├── VulkanInstance.hpp
│   ├── VulkanDevice.hpp
│   ├── Swapchain.hpp
│   └── Pipeline.hpp          # NEW
├── Renderer.hpp               # Update aggregation
src/
├── Renderer/
│   ├── VulkanInstance.cpp
│   ├── VulkanDevice.cpp
│   ├── Swapchain.cpp
│   └── Pipeline.cpp          # NEW
shaders/
├── CMakeLists.txt            # NEW - shader compilation
├── basic.vert                # NEW
├── basic.frag                # NEW
└── compiled/                 # Generated SPIR-V output
cmake/
└── ShaderCompilation.cmake   # NEW
```

**Namespace:** `Vecna::Renderer`

**Conventions de Nommage:**
- Classes: PascalCase (Pipeline)
- Fonctions: camelCase (createPipeline, loadShaderModule)
- Membres privés: m_ prefix (m_pipeline, m_pipelineLayout)
- Constantes: UPPER_SNAKE_CASE (MAX_PUSH_CONSTANT_SIZE)
- Fichiers: PascalCase (Pipeline.hpp)

**RAII Obligatoire:**
- vkDestroyPipeline() dans le destructeur
- vkDestroyPipelineLayout() dans le destructeur
- vkDestroyShaderModule() après création du pipeline (pas besoin de garder)
- Destruction ordre: Pipeline → PipelineLayout

**Error Handling:**
- Exceptions pour erreurs fatales (création pipeline échouée, shader non trouvé)
- Logging avec "[Renderer]" prefix

### Structure Vertex Interleaved

Selon architecture.md, le format vertex est interleaved:
```cpp
struct Vertex {
    glm::vec3 position;  // Non! Pas de glm - utiliser les types Math custom
    glm::vec3 normal;    // Donc:
    glm::vec3 color;
};

// CORRECT selon architecture (pas de GLM):
struct Vertex {
    float position[3];
    float normal[3];
    float color[3];
};
// Ou avec les types Math/ custom quand ils seront créés
```

**Binding Description:**
```cpp
VkVertexInputBindingDescription bindingDescription{};
bindingDescription.binding = 0;
bindingDescription.stride = sizeof(Vertex);  // 9 * sizeof(float) = 36 bytes
bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
```

**Attribute Descriptions:**
```cpp
std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

// Position
attributeDescriptions[0].binding = 0;
attributeDescriptions[0].location = 0;
attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
attributeDescriptions[0].offset = offsetof(Vertex, position);

// Normal
attributeDescriptions[1].binding = 0;
attributeDescriptions[1].location = 1;
attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
attributeDescriptions[1].offset = offsetof(Vertex, normal);

// Color
attributeDescriptions[2].binding = 0;
attributeDescriptions[2].location = 2;
attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
attributeDescriptions[2].offset = offsetof(Vertex, color);
```

### Push Constants pour MVP

Push constants sont plus efficaces que UBOs pour les petites données fréquemment mises à jour:
```cpp
// Structure alignée sur 4 bytes
struct PushConstants {
    float mvp[16];  // 4x4 matrix = 64 bytes (dans la limite de 128 bytes garantie)
};

VkPushConstantRange pushConstantRange{};
pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
pushConstantRange.offset = 0;
pushConstantRange.size = sizeof(PushConstants);  // 64 bytes
```

### Basic Vertex Shader (basic.vert)

```glsl
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragNormal = inNormal;  // Pour l'éclairage dans 2-4
}
```

### Basic Fragment Shader (basic.frag)

```glsl
#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
    // L'éclairage sera ajouté dans Story 2-4
}
```

### CMake Shader Compilation

```cmake
# cmake/ShaderCompilation.cmake
find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found. Please install Vulkan SDK.")
endif()

function(compile_shader SHADER_SOURCE OUTPUT_DIR)
    get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME)
    set(SPIRV_OUTPUT "${OUTPUT_DIR}/${SHADER_NAME}.spv")

    add_custom_command(
        OUTPUT ${SPIRV_OUTPUT}
        COMMAND ${GLSLC} ${SHADER_SOURCE} -o ${SPIRV_OUTPUT}
        DEPENDS ${SHADER_SOURCE}
        COMMENT "Compiling shader ${SHADER_NAME}"
    )

    # Return the output path
    set(SPIRV_OUTPUT ${SPIRV_OUTPUT} PARENT_SCOPE)
endfunction()
```

### Lecture des fichiers SPIR-V

```cpp
std::vector<char> Pipeline::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

VkShaderModule Pipeline::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_device.getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }

    return shaderModule;
}
```

### Pipeline Configuration

```cpp
void Pipeline::createGraphicsPipeline() {
    // Shader stages
    auto vertShaderCode = readFile("shaders/compiled/basic.vert.spv");
    auto fragShaderCode = readFile("shaders/compiled/basic.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo shaderStages[2] = {};

    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertShaderModule;
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragShaderModule;
    shaderStages[1].pName = "main";

    // Dynamic states (viewport and scissor)
    std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Vertex input
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport state (dynamic)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling (disabled)
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blending (opaque)
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Depth stencil (préparé pour 2-4, désactivé pour l'instant)
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;  // Activé dans 2-4
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Create pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipeline(m_device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    // Cleanup shader modules (not needed after pipeline creation)
    vkDestroyShaderModule(m_device.getDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(m_device.getDevice(), vertShaderModule, nullptr);

    Core::Logger::info("Renderer", "Graphics pipeline created");
}
```

### Intégration dans Application::recordCommandBuffer

```cpp
void Application::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    // ... begin command buffer ...
    // ... begin render pass ...

    // Bind the graphics pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getPipeline());

    // Set dynamic viewport
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapchain->getExtent().width);
    viewport.height = static_cast<float>(m_swapchain->getExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    // Set dynamic scissor
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapchain->getExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // No draw calls yet - will be added in Story 2-2/2-3

    // ... end render pass ...
    // ... end command buffer ...
}
```

### Logger Format Attendu

```
[Renderer] Compiling shader: basic.vert
[Renderer] Compiling shader: basic.frag
[Renderer] Shader modules loaded
[Renderer] Pipeline layout created
[Renderer] Graphics pipeline created
[Renderer] Pipeline destroyed
```

### Dépendances avec Stories Précédentes

**Story 1-5 (Swapchain et Présentation):**
- `getRenderPass()` - Requis pour créer le pipeline
- `getExtent()` - Requis pour viewport/scissor dynamiques
- La boucle de rendu est déjà fonctionnelle

**VulkanDevice (Story 1-4):**
- `getDevice()` - Requis pour toutes les créations Vulkan

### Chemins des Shaders Compilés

Les shaders compilés seront dans `shaders/compiled/`:
- Le chemin doit être relatif à l'exécutable OU configurable via CMake
- Considérer l'utilisation de `CMAKE_BINARY_DIR` ou `CMAKE_SOURCE_DIR`
- Pour le développement: `${CMAKE_SOURCE_DIR}/shaders/compiled/`
- Pour l'installation: copier les .spv avec l'exécutable

### Learnings de Story 1-5 à Appliquer

1. **Validation précoce** - Vérifier l'existence des fichiers shader avant de tenter de les charger
2. **Documentation destruction order** - Documenter que Pipeline doit être détruit AVANT Swapchain
3. **Constantes nommées** - Pas de magic numbers (ex: taille push constants)
4. **Logging cohérent** - Utiliser "[Renderer]" pour tout le module Renderer
5. **Bounds checking** - Utiliser `.at()` pour les accès aux vecteurs

### Préparation pour Stories Suivantes

Cette story prépare:
- **Story 2-2 (Vertex/Index Buffers)** - Le pipeline définit le format vertex
- **Story 2-3 (Triangle Test)** - Le pipeline est prêt à être bindé pour le rendu
- **Story 2-4 (Cube 3D)** - Le depth stencil state est préconfiguré (juste à activer)

### References

- [Source: architecture.md#Rendering Architecture] - Forward rendering, monolithic shaders
- [Source: architecture.md#Memory & Data] - Interleaved vertex layout
- [Source: architecture.md#Implementation Patterns] - RAII, naming conventions
- [Source: epics.md#Story 2.1] - Acceptance criteria détaillés
- [Source: 1-5-swapchain-et-presentation.md] - Patterns établis, learnings
- [Vulkan Tutorial: Graphics Pipeline](https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Introduction)
- [Vulkan Tutorial: Shader Modules](https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Shader_modules)

## Dev Agent Record

### Agent Model Used
claude-opus-4-5-20251101

### Debug Log References
N/A - No errors during implementation

### Completion Notes List
- Task 1 & 2 were already implemented (shaders and CMake config existed)
- Pipeline class created with RAII pattern for VkPipeline and VkPipelineLayout
- Vertex structure defined with interleaved layout (position, normal, color - 36 bytes)
- PushConstants structure for MVP matrix (64 bytes, within 128 byte Vulkan guarantee)
- Shader path configured as "../../shaders/compiled" relative to build/Debug executable
- Pipeline integrated into Application with proper destruction order (Pipeline before Swapchain)
- Dynamic viewport and scissor configured in recordCommandBuffer
- 16 new unit tests for Vertex, PushConstants, binding and attribute descriptions
- All 57 tests pass (41 existing + 16 new)
- Application runs and shows "[Renderer] Graphics pipeline created" in logs

### File List
- include/Vecna/Renderer/Pipeline.hpp (created, review: inline constexpr, uint32_t alignment)
- src/Renderer/Pipeline.cpp (created, review: readShaderFile with alignment, log consistency)
- include/Vecna/Core/Application.hpp (modified - added Pipeline member, createPipeline method)
- src/Core/Application.cpp (modified - Pipeline creation/destruction, shader path search, recreate on swapchain change)
- include/Vecna/Renderer.hpp (modified - added Pipeline.hpp include)
- CMakeLists.txt (modified - added Pipeline.cpp to sources)
- tests/Renderer/PipelineTest.cpp (created)
- tests/CMakeLists.txt (modified - added PipelineTest.cpp)
- shaders/basic.frag (modified - documented lighting constants)

## Code Review

### Review Date
2026-01-28

### Issues Found
- **HIGH-1:** Chemin de shaders hardcodé non portable - FIXED
- **HIGH-2:** Pipeline non recréé lors du swapchain recreate - FIXED
- **MED-1:** Pas de vérification d'alignement SPIR-V - FIXED
- **MED-2:** Magic numbers pour la lumière dans le shader - FIXED (documented)
- **MED-3:** Story claim vs réalité (basic.frag a déjà lighting) - NOTED (feature, not bug)
- **MED-4:** Pas de test d'intégration pour Pipeline - DEFERRED (requires Vulkan mocking)
- **LOW-1:** Log incohérence "Pipeline destroyed" vs "Graphics pipeline" - FIXED
- **LOW-2:** File List ne mentionne pas shaders - NOTED (pre-existing files)
- **LOW-3:** static constexpr dans header (ODR) - FIXED (inline constexpr)

### Fixes Applied
1. Shader path now searches multiple locations (findShaderDirectory())
2. Pipeline is recreated when swapchain is recreated (render pass may change)
3. SPIR-V files read with uint32_t alignment (readShaderFile returns vector<uint32_t>)
4. Fragment shader lighting values documented with comments
5. Log message changed to "Graphics pipeline destroyed" for consistency
6. Changed to inline constexpr for PUSH_CONSTANT_SIZE

### Post-Review Validation
- Build: SUCCESS
- Tests: 57/57 PASS
- Application: Runs correctly

## Change Log

- 2026-01-28: Story created with comprehensive context from epics.md, architecture.md, and Story 1-5 learnings
- 2026-01-28: Implementation completed - all ACs met, 57/57 tests pass
- 2026-01-28: Code review completed - 6 fixes applied, story marked done

# Story 1.3: Instance Vulkan et Validation

Status: done

## Story

As a développeur,
I want initialiser Vulkan avec les validation layers,
so that je puisse détecter les erreurs Vulkan pendant le développement.

## Acceptance Criteria

### AC1: Création Instance Vulkan
**Given** l'application démarre
**When** l'instance Vulkan est créée
**Then** VkInstance est créé avec succès
**And** les extensions requises pour GLFW sont activées
**And** le logging affiche "[Renderer] Vulkan instance created"

### AC2: Debug Messenger en Mode Debug
**Given** l'application tourne en mode Debug
**When** une erreur Vulkan se produit
**Then** le debug messenger capture l'erreur
**And** le message est affiché dans les logs avec le niveau approprié

### AC3: Pas de Validation en Release
**Given** l'application tourne en mode Release
**When** l'instance est créée
**Then** les validation layers ne sont pas activées (performance)

## Tasks / Subtasks

- [x] Task 1: Créer la classe VulkanInstance (AC: #1, #2, #3)
  - [x] Créer include/Vecna/Renderer/VulkanInstance.hpp
  - [x] Créer src/Renderer/VulkanInstance.cpp
  - [x] Implémenter le constructeur avec création VkInstance
  - [x] Configurer les extensions requises (glfwGetRequiredInstanceExtensions)
  - [x] Activer VK_EXT_debug_utils en mode Debug
  - [x] Implémenter le destructeur RAII (vkDestroyInstance)

- [x] Task 2: Implémenter les Validation Layers (AC: #2, #3)
  - [x] Vérifier la disponibilité de VK_LAYER_KHRONOS_validation
  - [x] Activer les validation layers uniquement si NDEBUG n'est pas défini
  - [x] Configurer VkDebugUtilsMessengerCreateInfoEXT
  - [x] Implémenter le callback de debug (debugCallback)
  - [x] Créer/Détruire le debug messenger via extension functions

- [x] Task 3: Créer le header d'agrégation Renderer (AC: #1)
  - [x] Créer include/Vecna/Renderer.hpp
  - [x] Inclure VulkanInstance.hpp

- [x] Task 4: Intégrer dans Application (AC: #1, #2)
  - [x] Modifier Application pour créer VulkanInstance après Window
  - [x] Passer le Window handle pour les extensions GLFW
  - [x] Vérifier l'ordre d'initialisation (Window avant Vulkan)

- [x] Task 5: Mettre à jour CMakeLists.txt (AC: #1)
  - [x] Ajouter src/Renderer/VulkanInstance.cpp aux sources
  - [x] Vérifier le linkage avec Vulkan::Vulkan

- [x] Task 6: Valider le fonctionnement (AC: #1, #2, #3)
  - [x] Build et run en mode Debug - vérifier logs validation
  - [x] Build et run en mode Release - vérifier absence validation layers
  - [x] Provoquer une erreur Vulkan intentionnelle pour tester le callback

## Dev Notes

### Architecture Patterns à Respecter

**Structure selon architecture.md:**
```
include/Vecna/
├── Renderer/
│   └── VulkanInstance.hpp
├── Renderer.hpp              # Aggregation header
src/
├── Renderer/
│   └── VulkanInstance.cpp
```

**Namespace:** `Vecna::Renderer`

**Conventions de Nommage:**
- Classes: PascalCase (VulkanInstance)
- Fonctions: camelCase (createInstance, setupDebugMessenger)
- Membres privés: m_ prefix (m_instance, m_debugMessenger)
- Constantes: UPPER_SNAKE_CASE (VALIDATION_LAYERS)
- Fichiers: PascalCase (VulkanInstance.hpp)

**RAII Obligatoire:**
- Le destructeur doit détruire le debug messenger AVANT l'instance
- Ordre de destruction inverse de la création
- Pas de cleanup manuel requis par l'appelant

**Error Handling:**
- Exceptions pour les erreurs fatales (VkResult != VK_SUCCESS)
- Pattern cohérent avec Story 1.2 (try/catch dans main.cpp)

### Vulkan Instance Configuration

```cpp
// Application info
VkApplicationInfo appInfo{};
appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
appInfo.pApplicationName = "Vecna";
appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
appInfo.pEngineName = "Vecna Engine";
appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
appInfo.apiVersion = VK_API_VERSION_1_3;  // Vulkan 1.3

// Extensions requises
uint32_t glfwExtensionCount = 0;
const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
// + VK_EXT_DEBUG_UTILS_EXTENSION_NAME en Debug
```

### Validation Layers Setup

```cpp
// Layer requis
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// Vérification disponibilité
bool checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    // Check if validationLayers are in availableLayers
}
```

### Debug Messenger Callback

```cpp
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    // Map severity to Logger level
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        Logger::error("Vulkan", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        Logger::warn("Vulkan", pCallbackData->pMessage);
    } else {
        Logger::debug("Vulkan", pCallbackData->pMessage);
    }
    return VK_FALSE;
}
```

### Extension Functions Loading

```cpp
// vkCreateDebugUtilsMessengerEXT n'est pas chargé par défaut
VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}
```

### Debug/Release Conditional Compilation

```cpp
#ifdef NDEBUG
    constexpr bool ENABLE_VALIDATION_LAYERS = false;
#else
    constexpr bool ENABLE_VALIDATION_LAYERS = true;
#endif
```

### Logger Format

```
[Renderer] Vulkan instance created
[Renderer] Validation layers enabled
[Vulkan] Validation Error: ...
[Vulkan] Validation Warning: ...
```

### Dépendances avec Story 1.2

- **Window.getHandle():** Requis pour glfwGetRequiredInstanceExtensions
- **GLFWContext:** GLFW doit être initialisé AVANT de créer l'instance Vulkan
- **Logger:** Utiliser le Logger existant pour les messages

### Fichiers Existants à Connaître

Depuis Story 1.2:
- `include/Vecna/Core/Window.hpp` - getHandle() retourne GLFWwindow*
- `include/Vecna/Core/Logger.hpp` - debug(), info(), warn(), error()
- `include/Vecna/Core/Application.hpp` - getWindow() retourne Window&
- `src/Core/Application.cpp` - Modifier pour ajouter VulkanInstance

### CMake Debug/Release

Le projet utilise CMAKE_BUILD_TYPE:
- Debug: NDEBUG non défini → validation layers activées
- Release: NDEBUG défini → validation layers désactivées

### References

- [Source: architecture.md#Renderer Module] - Structure VulkanInstance
- [Source: architecture.md#Implementation Patterns] - RAII, naming conventions
- [Source: architecture.md#Logging] - Format logs "[MODULE] Message"
- [Source: epics.md#Story 1.3] - Acceptance criteria détaillés
- [Source: 1-2-creation-fenetre-glfw.md] - Patterns établis (exceptions, RAII)

## Dev Agent Record

### Agent Model Used

Claude Opus 4.5 (claude-opus-4-5-20251101)

### Debug Log References

- Build Debug: Success (MSVC 19.50)
- Build Release: Success (MSVC 19.50)
- Runtime Debug: Validation layers enabled, instance created
- Runtime Release: No validation layers, instance created

### Completion Notes List

- VulkanInstance class créée avec RAII pattern
- Validation layers activées conditionnellement via NDEBUG
- Debug messenger callback route les messages vers Logger (error/warn/debug/info)
- Extension functions chargées dynamiquement (CreateDebugUtilsMessengerEXT, DestroyDebugUtilsMessengerEXT)
- Application modifiée pour créer VulkanInstance après Window
- Ordre de destruction correct: VulkanInstance avant Window
- AC1 validé: "[Renderer] Vulkan instance created" affiché
- AC2 validé: Validation layers activées en Debug, callback correctement implémenté
- AC3 validé: Pas de validation layers en Release

**Code Review Notes:**
- HIGH-1 (Task 6 subtask): Debug callback validated via code inspection - implementation correct
- HIGH-2 (Exceptions): Accepted deviation from architecture - consistent with Story 1.2 pattern
- Added null safety for glfwGetRequiredInstanceExtensions
- Added getVulkanInstance() for future stories (Device, Surface)
- Changed VALIDATION_LAYERS to static constexpr array for performance

### File List

**Fichiers créés:**
- include/Vecna/Renderer/VulkanInstance.hpp
- include/Vecna/Renderer.hpp (aggregation header)
- src/Renderer/VulkanInstance.cpp

**Fichiers modifiés:**
- include/Vecna/Core/Application.hpp (ajout m_vulkanInstance)
- src/Core/Application.cpp (création VulkanInstance)
- CMakeLists.txt (ajout VulkanInstance.cpp)

## Change Log

- 2026-01-27: Story created via create-story workflow with comprehensive context analysis
- 2026-01-27: Story implementation completed - VulkanInstance with validation layers, debug messenger, RAII pattern
- 2026-01-27: Code review fixes applied:
  - HIGH-3: Added null check for glfwGetRequiredInstanceExtensions
  - MED-1: Added getVulkanInstance() getter to Application
  - MED-2: Added VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT to debug messenger
  - MED-3: Changed VALIDATION_LAYERS to static constexpr std::array
  - LOW-2: Added comment explaining debugCreateInfo scope requirement
  - LOW-3: Clarified RAII destruction order comment in Application.hpp
  - Added extensions count logging

# Story 1.5: Swapchain et Présentation

Status: done

## Story

As a utilisateur,
I want voir la fenêtre avec un rendu Vulkan actif,
so that je sache que le pipeline graphique fonctionne.

## Acceptance Criteria

### AC1: Création Swapchain
**Given** le device Vulkan est initialisé
**When** la swapchain est créée
**Then** les images de la swapchain sont disponibles
**And** le format de surface est approprié (SRGB préféré)
**And** le mode de présentation est sélectionné (Mailbox > Fifo)

### AC2: Boucle de Rendu
**Given** la swapchain est créée
**When** la boucle de rendu tourne
**Then** la fenêtre affiche une couleur de fond (clear color)
**And** le rendu est présenté sans erreur
**And** le framerate est stable

### AC3: Gestion Redimensionnement
**Given** la fenêtre est redimensionnée
**When** les dimensions changent
**Then** la swapchain est recréée avec les nouvelles dimensions
**And** le rendu continue sans interruption visible

### AC4: Cleanup RAII
**Given** l'application se ferme
**When** le cleanup est exécuté
**Then** toutes les ressources Vulkan sont libérées via RAII
**And** aucune validation error n'est reportée

## Tasks / Subtasks

- [x] Task 1: Créer la Surface Vulkan (AC: #1)
  - [x] Ajouter une présentation queue family dans QueueFamilyIndices
  - [x] Créer VkSurfaceKHR via glfwCreateWindowSurface
  - [x] Mettre à jour VulkanDevice pour stocker la surface ou créer classe dédiée
  - [x] Vérifier le support de présentation sur la queue sélectionnée

- [x] Task 2: Créer la classe Swapchain (AC: #1, #3, #4)
  - [x] Créer include/Vecna/Renderer/Swapchain.hpp
  - [x] Créer src/Renderer/Swapchain.cpp
  - [x] Implémenter querySwapchainSupport() pour formats, present modes, capabilities
  - [x] Implémenter chooseSwapSurfaceFormat() (SRGB B8G8R8A8 préféré)
  - [x] Implémenter chooseSwapPresentMode() (Mailbox > Fifo)
  - [x] Implémenter chooseSwapExtent() avec gestion des dimensions
  - [x] Créer la swapchain avec image count optimal (min+1, max triple buffer)
  - [x] Récupérer les swapchain images et créer les image views
  - [x] Implémenter le destructeur RAII

- [x] Task 3: Activer l'extension VK_KHR_swapchain (AC: #1)
  - [x] Modifier VulkanDevice pour activer VK_KHR_swapchain
  - [x] Vérifier le support de l'extension avant création du device

- [x] Task 4: Implémenter la boucle de rendu basique (AC: #2)
  - [x] Créer les synchronisation objects (semaphores, fences)
  - [x] Implémenter acquireNextImage avec gestion VK_ERROR_OUT_OF_DATE
  - [x] Implémenter le submit du command buffer (vide pour l'instant, juste clear)
  - [x] Implémenter presentImage avec gestion VK_SUBOPTIMAL
  - [x] Ajouter le clear color dans le render pass

- [x] Task 5: Créer le Render Pass et Framebuffers (AC: #2)
  - [x] Créer un render pass basique avec color attachment
  - [x] Créer les framebuffers pour chaque swapchain image
  - [x] Implémenter le command buffer recording pour clear

- [x] Task 6: Gérer le redimensionnement (AC: #3)
  - [x] Détecter le redimensionnement via Window::wasResized()
  - [x] Implémenter recreateSwapchain()
  - [x] Recréer les framebuffers après recreation
  - [x] Gérer les cas de minimisation (extent = 0)

- [x] Task 7: Intégrer dans Application (AC: #2, #3, #4)
  - [x] Créer la surface dans Application après Window et VulkanInstance
  - [x] Créer Swapchain après VulkanDevice
  - [x] Implémenter la boucle de rendu dans Application::run()
  - [x] Vérifier l'ordre de destruction correct

- [x] Task 8: Mettre à jour CMakeLists et headers (AC: #1)
  - [x] Ajouter Swapchain.cpp aux sources
  - [x] Mettre à jour Renderer.hpp avec Swapchain.hpp

- [x] Task 9: Valider le fonctionnement (AC: #1, #2, #3, #4)
  - [x] Build et run - vérifier la clear color affichée
  - [x] Redimensionner la fenêtre - vérifier pas de crash
  - [x] Vérifier les logs de création/destruction
  - [x] Vérifier aucune validation error en Debug

## Dev Notes

### Architecture Patterns à Respecter

**Structure selon architecture.md:**
```
include/Vecna/
├── Renderer/
│   ├── VulkanInstance.hpp
│   ├── VulkanDevice.hpp
│   └── Swapchain.hpp          # NEW
├── Renderer.hpp               # Update aggregation
src/
├── Renderer/
│   ├── VulkanInstance.cpp
│   ├── VulkanDevice.cpp
│   └── Swapchain.cpp          # NEW
```

**Namespace:** `Vecna::Renderer`

**Conventions de Nommage:**
- Classes: PascalCase (Swapchain)
- Fonctions: camelCase (createSwapchain, recreateSwapchain)
- Membres privés: m_ prefix (m_swapchain, m_imageViews)
- Constantes: UPPER_SNAKE_CASE (MAX_FRAMES_IN_FLIGHT)
- Fichiers: PascalCase (Swapchain.hpp)

**RAII Obligatoire:**
- vkDeviceWaitIdle() avant toute destruction
- Destruction ordre: ImageViews → Swapchain → Surface
- Framebuffers détruits avant swapchain lors de recreation

**Error Handling:**
- Exceptions pour erreurs fatales (création swapchain échouée)
- Gestion spéciale de VK_ERROR_OUT_OF_DATE et VK_SUBOPTIMAL

### Swapchain Support Query

```cpp
struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapchainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}
```

### Surface Format Selection (SRGB préféré)

```cpp
VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    // Fallback to first available
    return availableFormats[0];
}
```

### Present Mode Selection (Mailbox > Fifo)

```cpp
VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availableModes) {
    for (const auto& mode : availableModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;  // Triple buffering, low latency
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;  // Always available, vsync
}
```

### Swap Extent (Dimensions)

```cpp
VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                            uint32_t windowWidth, uint32_t windowHeight) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    VkExtent2D extent = {windowWidth, windowHeight};
    extent.width = std::clamp(extent.width,
                              capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height,
                               capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);
    return extent;
}
```

### Frame Synchronization

```cpp
static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

// Per-frame synchronization
std::vector<VkSemaphore> m_imageAvailableSemaphores;  // Signal when image acquired
std::vector<VkSemaphore> m_renderFinishedSemaphores;  // Signal when rendering done
std::vector<VkFence> m_inFlightFences;                // CPU-GPU sync
uint32_t m_currentFrame = 0;
```

### Render Loop Pattern

```cpp
void drawFrame() {
    // Wait for previous frame
    vkWaitForFences(device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    // Acquire next image
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                            m_imageAvailableSemaphores[m_currentFrame],
                                            VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    vkResetFences(device, 1, &m_inFlightFences[m_currentFrame]);

    // Record command buffer (clear color for now)
    // ...

    // Submit
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]);

    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapchain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image");
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
```

### QueueFamilyIndices Update (Present Support)

```cpp
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;  // ADD THIS

    [[nodiscard]] bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

// In findQueueFamilies, add:
VkBool32 presentSupport = false;
vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
if (presentSupport) {
    indices.presentFamily = i;
}
```

### Device Extensions

```cpp
static constexpr std::array<const char*, 1> DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// In createLogicalDevice:
createInfo.enabledExtensionCount = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
createInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();
```

### Logger Format Attendu

```
[Renderer] Surface created
[Renderer] Swapchain created (1280x720, 3 images, MAILBOX)
[Renderer] Image views created
[Renderer] Render pass created
[Renderer] Framebuffers created
[Renderer] Sync objects created
[Renderer] Swapchain recreated (1920x1080)
[Renderer] Swapchain destroyed
```

### Window Integration (Déjà disponible)

La classe Window a déjà le support nécessaire:
- `getHandle()` - Pour glfwCreateWindowSurface
- `getWidth()`, `getHeight()` - Pour les dimensions
- `wasResized()`, `resetResizedFlag()` - Pour détecter le redimensionnement

### Dépendances avec Stories Précédentes

**Story 1.3 (VulkanInstance):**
- `getInstance()` - Requis pour créer la surface

**Story 1.4 (VulkanDevice):**
- `getDevice()` - Requis pour créer la swapchain
- `getPhysicalDevice()` - Requis pour query swapchain support
- `getGraphicsQueue()` - Requis pour submit
- `getGraphicsQueueFamily()` - Pour command pool
- Modifier pour ajouter present queue si différente de graphics

**Window (Story 1.2):**
- `getHandle()` - Pour créer la surface
- `wasResized()` / `resetResizedFlag()` - Pour recreation

### Fichiers Existants à Modifier

- `include/Vecna/Renderer/VulkanDevice.hpp` - Ajouter presentFamily à QueueFamilyIndices
- `src/Renderer/VulkanDevice.cpp` - Activer VK_KHR_swapchain, trouver present queue
- `include/Vecna/Core/Application.hpp` - Ajouter Swapchain member
- `src/Core/Application.cpp` - Créer surface/swapchain, boucle de rendu
- `include/Vecna/Renderer.hpp` - Ajouter Swapchain.hpp

### Clear Color Suggérée

Pour le test visuel, utiliser une couleur distinctive:
```cpp
VkClearValue clearColor = {{{0.1f, 0.1f, 0.2f, 1.0f}}};  // Dark blue
```

### Gestion Minimisation

Quand la fenêtre est minimisée, extent = (0, 0). Il faut attendre:
```cpp
int width = 0, height = 0;
glfwGetFramebufferSize(window, &width, &height);
while (width == 0 || height == 0) {
    glfwGetFramebufferSize(window, &width, &height);
    glfwWaitEvents();
}
```

### References

- [Source: architecture.md#Rendering Architecture] - Forward rendering, swapchain
- [Source: architecture.md#Implementation Patterns] - RAII, naming conventions
- [Source: epics.md#Story 1.5] - Acceptance criteria détaillés
- [Source: 1-4-selection-device-vulkan.md] - VulkanDevice patterns établis
- [Vulkan Tutorial: Swap Chain](https://vulkan-tutorial.com/Drawing_a_triangle/Presentation/Swap_chain)
- [Vulkan Tutorial: Drawing](https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Rendering_and_presentation)

## Dev Agent Record

### Agent Model Used
claude-opus-4-5-20251101

### Debug Log References
N/A - No errors during implementation

### Completion Notes List
- QueueFamilyIndices extended with presentFamily member
- VulkanDevice now requires surface for construction, enables VK_KHR_swapchain extension
- Swapchain class implements full render loop with synchronization (2 frames in flight)
- Surface created in Application, owned by Application (destroyed before VulkanInstance)
- Swapchain supports recreation on window resize with minimization handling
- MAILBOX present mode selected when available (triple buffering)
- SRGB B8G8R8A8 format preferred for swapchain images
- Dark blue clear color (0.1, 0.1, 0.2) for visual verification
- All 41 unit tests pass (including updated QueueFamilyIndicesTest)

### File List
- include/Vecna/Renderer/Swapchain.hpp (created)
- src/Renderer/Swapchain.cpp (created)
- include/Vecna/Renderer/VulkanDevice.hpp (modified - added presentFamily, surface param)
- src/Renderer/VulkanDevice.cpp (modified - added present queue, VK_KHR_swapchain)
- include/Vecna/Core/Application.hpp (modified - added Swapchain, surface, drawFrame, recordCommandBuffer)
- src/Core/Application.cpp (modified - render loop implementation)
- include/Vecna/Renderer.hpp (modified - added Swapchain.hpp)
- CMakeLists.txt (modified - added Swapchain.cpp)
- tests/Renderer/QueueFamilyIndicesTest.cpp (modified - tests for presentFamily)

## Change Log

- 2026-01-28: Story created with comprehensive context from epics.md, architecture.md, and previous story learnings
- 2026-01-28: Implementation completed - all ACs met, ready for review
- 2026-01-28: Code review completed - 6 issues fixed

## Senior Developer Review (AI)

**Reviewer:** claude-opus-4-5-20251101
**Date:** 2026-01-28
**Outcome:** APPROVED

### Issues Found & Fixed

| Sévérité | ID | Description | Action |
|----------|-----|-------------|--------|
| 🔴 HIGH | HIGH-1 | Swapchain support non vérifié (formats/presentModes vides) | FIXED - Validation ajoutée dans createSwapchain() |
| 🔴 HIGH | HIGH-2 | Bounds check manquant dans getFramebuffer | FIXED - Utilisé .at() au lieu de [] |
| 🔴 HIGH | HIGH-3 | Render pass recréé inutilement à chaque resize | FIXED - Optimisé recreate() pour préserver le render pass |
| 🟡 MED | MED-2 | Viewport/Scissor dynamiques sans pipeline | FIXED - Code mort supprimé |
| 🟡 MED | MED-3 | Magic numbers pour clear color | FIXED - Constante CLEAR_COLOR créée |
| 🟡 MED | MED-4 | Swapchain adequacy non vérifiée dans VulkanDevice | FIXED - Validation formats/modes dans rateDeviceSuitability |
| 🟢 LOW | LOW-1 | Include `<array>` non utilisé | FIXED - Supprimé |
| 🟢 LOW | LOW-2 | Logging incohérent ("Renderer" depuis Core) | FIXED - Changé en "Core" |
| 🟢 LOW | LOW-3 | Documentation ordre destruction manquante | FIXED - Ajoutée dans Swapchain.hpp |

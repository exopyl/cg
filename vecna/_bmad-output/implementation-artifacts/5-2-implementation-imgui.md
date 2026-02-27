# Story 5.2: Implémentation ImGUI

Status: done

## Story

As a développeur,
I want intégrer Dear ImGUI avec Vulkan,
so that je puisse afficher des éléments d'interface utilisateur.

## Acceptance Criteria

1. **AC1 - Contexte ImGUI créé et backends configurés**
   - Given ImGUI est disponible via FetchContent
   - When ImGuiRenderer est initialisé
   - Then le contexte ImGUI est créé
   - And les backends GLFW et Vulkan sont configurés
   - And le logging affiche "[UI] ImGui initialized"

2. **AC2 - Frame ImGUI démarre correctement**
   - Given ImGuiRenderer est initialisé
   - When beginFrame() est appelé
   - Then un nouveau frame ImGUI commence
   - And les inputs sont capturés depuis GLFW

3. **AC3 - Rendu ImGUI intégré au command buffer**
   - Given des éléments UI sont créés
   - When render() est appelé
   - Then les draw commands ImGUI sont générées
   - And le rendu est intégré au command buffer Vulkan

4. **AC4 - Libération propre des ressources**
   - Given l'application se ferme
   - When ImGuiRenderer est détruit
   - Then toutes les ressources ImGUI sont libérées
   - And aucune fuite mémoire n'est reportée

## Tasks / Subtasks

- [x] Task 1: Valider AC1 — ImGui FetchContent et initialisation (AC: #1)
  - [x] 1.1 Vérifier que ImGui v1.90.1 est fetchée via FetchContent dans `cmake/FetchDependencies.cmake`
  - [x] 1.2 Vérifier que `ImGuiRenderer::init()` crée le contexte ImGui (`IMGUI_CHECKVERSION`, `CreateContext`)
  - [x] 1.3 Vérifier que les backends GLFW (`ImGui_ImplGlfw_InitForVulkan`) et Vulkan (`ImGui_ImplVulkan_Init`) sont configurés
  - [x] 1.4 Vérifier que `Logger::info("UI", "ImGui initialized")` est appelé après init
  - [x] 1.5 Vérifier que le descriptor pool est créé avec les bons paramètres (FREE_DESCRIPTOR_SET_BIT, 10 par type, maxSets=100)

- [x] Task 2: Valider AC2 — beginFrame et capture d'inputs (AC: #2)
  - [x] 2.1 Vérifier que `beginFrame()` appelle `ImGui_ImplVulkan_NewFrame()`, `ImGui_ImplGlfw_NewFrame()`, `ImGui::NewFrame()`
  - [x] 2.2 Vérifier que les inputs GLFW sont capturés via `install_callbacks=true` dans l'init
  - [x] 2.3 Vérifier que `WantCaptureMouse` est consulté dans les callbacks Application pour éviter les conflits d'input

- [x] Task 3: Valider AC3 — Rendu dans le command buffer (AC: #3)
  - [x] 3.1 Vérifier que `render(VkCommandBuffer)` appelle `ImGui::Render()` puis `ImGui_ImplVulkan_RenderDrawData()`
  - [x] 3.2 Vérifier que `beginFrame()` + `render()` sont appelés dans `recordCommandBuffer()` (Application.cpp)
  - [x] 3.3 Vérifier que le rendu ImGui est après le rendu 3D et avant `vkCmdEndRenderPass`

- [x] Task 4: Valider AC4 — Shutdown propre (AC: #4)
  - [x] 4.1 Vérifier que `shutdown()` appelle `ImGui_ImplVulkan_Shutdown()`, `ImGui_ImplGlfw_Shutdown()`, `ImGui::DestroyContext()` dans cet ordre
  - [x] 4.2 Vérifier que le descriptor pool est détruit après le shutdown ImGui
  - [x] 4.3 Vérifier que `m_uiRenderer.reset()` est appelé avant la destruction des ressources Vulkan dans `~Application()`
  - [x] 4.4 Vérifier que `vkDeviceWaitIdle` est appelé avant le shutdown

- [x] Task 5: Écrire les tests unitaires (AC: #1, #2, #3, #4)
  - [x] 5.1 Test: ImGuiRenderer est final (ne peut pas être sous-classé)
  - [x] 5.2 Test: ImGuiRenderer est destructible (std::is_destructible)
  - [x] 5.3 Test: ImGuiRenderer non-copyable et non-movable (RAII, références aux dépendances)
  - [x] 5.4 Test (existant): ImGuiRenderer hérite de IUIRenderer (déjà couvert dans 5-1)
  - [x] 5.5 Test: ImGuiRenderer a un destructeur virtuel (hérité d'IUIRenderer)
  - [x] 5.6 Test: ImGuiRenderer n'est pas default-constructible (requiert 4 dépendances)

- [x] Task 6: Validation compilation et régression (AC: #1, #2, #3, #4)
  - [x] 6.1 Compilation Debug sans erreurs (vecna.exe + vecna_tests.exe)
  - [x] 6.2 86/86 tests passent sans régression (80 existants + 6 nouveaux)
  - [x] 6.3 Vérification manuelle : application démarre, "[UI] ImGui initialized" loggé, main loop ok

## Dev Notes

### Contexte — Story déjà implémentée en 5-1

**IMPORTANT** : La totalité du code ImGui a été implémentée dans Story 5-1 lors de la création de l'abstraction. Il était impossible de valider l'abstraction IUIRenderer sans implémenter le concret ImGuiRenderer. Par conséquent, cette story est principalement une **validation et vérification** que l'implémentation existante satisfait les ACs de 5-2.

Le code ImGui existant dans `src/UI/ImGuiRenderer.cpp` couvre déjà :
- AC1 : Contexte créé, backends configurés, logging présent
- AC2 : beginFrame() avec NewFrame + capture inputs
- AC3 : render() avec Render + RenderDrawData dans le command buffer
- AC4 : shutdown() avec destruction ordonnée, m_uiRenderer.reset() avant Vulkan cleanup

### Fichiers existants (créés en Story 5-1)

| Fichier | Rôle |
|---------|------|
| `include/Vecna/UI/IUIRenderer.hpp` | Interface abstraite (5 méthodes virtuelles) |
| `include/Vecna/UI/ImGuiRenderer.hpp` | Implémentation concrète ImGui (final) |
| `src/UI/ImGuiRenderer.cpp` | Code ImGui complet (init, shutdown, beginFrame, render, menu) |
| `include/Vecna/UI.hpp` | Header d'agrégation du module UI |
| `tests/UI/UIAbstractionTest.cpp` | 6 tests compile-time de l'abstraction |

### Configuration ImGui (cmake/FetchDependencies.cmake)

- **Version** : v1.90.1 (pinned via GIT_TAG)
- **Build** : Static library compilée avec les backends GLFW + Vulkan
- **Dépendances** : `glfw`, `Vulkan::Vulkan` (linkées publiquement)
- **Warnings** : Supprimés pour le code tiers (MSVC: /W0, GCC: -w)

### Architecture ImGuiRenderer::init()

Paramètres nécessaires (injectés au constructeur) :
- `VulkanInstance&` → `initInfo.Instance`
- `VulkanDevice&` → PhysicalDevice, Device, QueueFamily, Queue
- `Swapchain&` → ImageCount, RenderPass
- `Window&` → GLFW handle pour `ImGui_ImplGlfw_InitForVulkan`

Descriptor pool : `VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT`, 10 par type × 11 types, maxSets=100

### Ordre critique d'initialisation/destruction

**Init** :
1. Application installe ses callbacks GLFW (trackball, pan, zoom)
2. ImGuiRenderer créé avec dépendances injectées
3. `setOnFileOpen()` appelé sur le type concret avant upcast
4. `m_uiRenderer = std::move(imguiRenderer)` (upcast vers IUIRenderer)
5. `m_uiRenderer->init()` → ImGui chaîne sur les callbacks existants (`install_callbacks=true`)

**Shutdown** :
1. `vkDeviceWaitIdle()` — attendre la fin du GPU
2. `m_uiRenderer.reset()` — ImGui shutdown (Vulkan backend → GLFW backend → contexte → descriptor pool)
3. Geometry buffers → Pipeline → Swapchain → VulkanDevice → Surface → VulkanInstance → Window

### Callback GLFW et WantCaptureMouse

Les 3 callbacks (mouseButton, cursorPos, scroll) dans Application.cpp vérifient `ImGui::GetIO().WantCaptureMouse` avant de traiter les inputs. Cela nécessite `#include <imgui.h>` dans Application.cpp — dette technique documentée et acceptée (Story 5-1 code review).

### Learnings de Story 5-1

- `Logger::info` doit être qualifié `Core::Logger::info` depuis le namespace `Vecna::UI`
- `${CMAKE_SOURCE_DIR}/..` ajouté aux include paths des tests pour cgmath
- `setOnFileOpen` gardé sur ImGuiRenderer (pas IUIRenderer) pour garder l'interface propre
- `onSwapchainRecreated()` ajouté comme hook avec implémentation vide par défaut
- L'ordre d'installation des callbacks GLFW est critique : Application d'abord, puis ImGui chaîne

### Conventions de code

- Namespace : `Vecna::UI`
- Nommage : PascalCase classes, camelCase méthodes, `m_` préfixe membres
- RAII : unique_ptr avec destruction explicite ordonnée
- Logging : `Core::Logger::info("UI", "message")`
- Tests : type traits compile-time (std::is_abstract, std::is_base_of, etc.)

### Ce qui reste à faire dans cette story

La story se résume à :
1. **Vérifier** que le code existant satisfait chaque AC (audit task par task)
2. **Ajouter 2-3 tests** supplémentaires spécifiques à ImGuiRenderer (final, destructible, constructeur)
3. **Compiler et valider** zéro régression
4. **Vérification manuelle** : UI fonctionne runtime (menu, inputs, shutdown propre)

### Project Structure Notes

- Tous les fichiers sont alignés avec l'architecture (`include/Vecna/UI/`, `src/UI/`)
- Le module UI suit la même convention que Core et Renderer
- Pas de nouveau fichier source à créer — seulement des tests à ajouter

### References

- [Source: architecture.md#Project Structure] — UI module layout
- [Source: architecture.md#Module Boundaries] — UI depends on Core, Scene, Renderer
- [Source: 5-1-abstraction-interface-ui.md] — Implémentation existante et code review
- [Source: cmake/FetchDependencies.cmake:38-75] — ImGui v1.90.1 FetchContent config
- [Source: src/UI/ImGuiRenderer.cpp] — Implémentation complète ImGui
- [Source: src/Core/Application.cpp:109-114] — Initialisation ImGuiRenderer
- [Source: src/Core/Application.cpp:128] — Shutdown m_uiRenderer.reset()
- [Source: src/Core/Application.cpp:382-383] — beginFrame + render dans recordCommandBuffer

## Dev Agent Record

### Agent Model Used

Claude Opus 4.6

### Debug Log References

- Aucune erreur de build — tous les fichiers existants de Story 5-1 satisfont déjà les ACs

### Completion Notes List

- Story 5-2 est une validation de l'implémentation existante créée en Story 5-1
- Tasks 1-4 : Audit code complet — toutes les ACs vérifiées ligne par ligne dans le code source
- AC1 : ImGui v1.90.1 via FetchContent, contexte créé, backends GLFW+Vulkan configurés, logging "[UI] ImGui initialized"
- AC2 : beginFrame() appelle NewFrame des 3 backends, inputs capturés via install_callbacks=true, WantCaptureMouse vérifié
- AC3 : render() appelle ImGui::Render() + RenderDrawData, intégré après rendu 3D et avant vkCmdEndRenderPass
- AC4 : shutdown() dans l'ordre correct (Vulkan→GLFW→Context→DescriptorPool), vkDeviceWaitIdle avant reset, m_uiRenderer.reset() avant VulkanDevice
- Task 5 : 6 nouveaux tests compile-time ajoutés (final, destructible, non-copyable, non-movable, virtual destructor, not-default-constructible)
- Task 6 : 86/86 tests passent, compilation Debug OK, zéro régression
- Vérification manuelle runtime effectuée : app démarre, ImGui initialized, main loop fonctionne
- AC4 "aucune fuite mémoire" vérifié par audit de code (destruction ordonnée correcte) — test runtime non applicable sans GPU mock

### Code Review Fixes (2026-02-27)

- [M1] Vérification manuelle exécutée : application lancée, "[UI] ImGui initialized" confirmé, main loop ok
- [M2] Ajouté test `ImGuiRendererIsNotDefaultConstructible` — prouve que 4 dépendances sont requises
- [M3] Documenté que AC4 "aucune fuite" est vérifié par audit de code (test runtime non faisable sans GPU mock)
- [L2] Retiré `#include <vulkan/vulkan.h>` redondant de ImGuiRenderer.hpp (déjà inclus via IUIRenderer.hpp)

### Change Log

- 2026-02-27: Story 5-2 validation — audit code existant + 6 tests ImGuiRenderer ajoutés
- 2026-02-27: Code review fixes — vérification manuelle, test constructeur, include cleanup

### File List

- tests/UI/UIAbstractionTest.cpp (modified) — Added 6 ImGuiRenderer compile-time tests
- include/Vecna/UI/ImGuiRenderer.hpp (modified) — Removed redundant vulkan.h include

# Story 5.1: Abstraction Interface UI

Status: done

## Story

As a développeur,
I want une interface abstraite pour le système UI,
so that je puisse changer de framework UI sans modifier le code métier.

## Acceptance Criteria

1. **AC1 - Interface IUIRenderer définie**
   - Given l'interface IUIRenderer est définie
   - When une implémentation est créée
   - Then les méthodes init(), shutdown(), beginFrame(), render(VkCommandBuffer), onSwapchainRecreated() sont disponibles
   - And l'interface ne dépend pas d'ImGUI directement

2. **AC2 - Isolation du code métier**
   - Given l'abstraction UI existe
   - When le code métier utilise l'UI
   - Then il interagit uniquement via IUIRenderer
   - And aucune dépendance directe à ImGUI dans les autres modules

3. **AC3 - Interchangeabilité du framework**
   - Given l'architecture est modulaire
   - When on souhaite changer de framework UI
   - Then seule l'implémentation de IUIRenderer doit être modifiée
   - And le reste du code reste inchangé

## Tasks / Subtasks

- [x] Task 1: Créer l'interface abstraite IUIRenderer (AC: #1)
  - [x] 1.1 Créer le header `include/Vecna/UI/IUIRenderer.hpp` avec la classe abstraite pure
  - [x] 1.2 Définir les méthodes virtuelles pures: `init()`, `shutdown()`, `beginFrame()`, `render(VkCommandBuffer)`
  - [x] 1.3 Ajouter `virtual ~IUIRenderer() = default;` pour destruction polymorphique
  - [x] 1.4 Vérifier: aucun `#include` ImGui dans ce header

- [x] Task 2: Créer l'en-tête d'agrégation UI (AC: #1)
  - [x] 2.1 Créer `include/Vecna/UI.hpp` qui inclut `UI/IUIRenderer.hpp`

- [x] Task 3: Intégrer IUIRenderer dans Application (AC: #2)
  - [x] 3.1 Forward-declare `Vecna::UI::IUIRenderer` dans Application.hpp
  - [x] 3.2 Ajouter membre `std::unique_ptr<UI::IUIRenderer> m_uiRenderer;` dans Application
  - [x] 3.3 Remplacer les appels directs ImGui dans `recordCommandBuffer()` par `m_uiRenderer->beginFrame()`, `m_uiRenderer->render(cmd)`
  - [x] 3.4 Remplacer `initImGui()`/`shutdownImGui()` par `m_uiRenderer->init()` / `m_uiRenderer.reset()` dans constructeur/destructeur
  - [x] 3.5 Supprimé `<imgui_impl_glfw.h>` et `<imgui_impl_vulkan.h>` de Application.cpp (conservé `<imgui.h>` pour WantCaptureMouse — dette technique documentée)

- [x] Task 4: Créer ImGuiRenderer implémentant IUIRenderer (AC: #1, #3)
  - [x] 4.1 Créer `include/Vecna/UI/ImGuiRenderer.hpp` - classe concrète héritant de IUIRenderer
  - [x] 4.2 Créer `src/UI/ImGuiRenderer.cpp` - code ImGui déplacé de Application.cpp
  - [x] 4.3 Implémenter `init()` avec le code de `Application::initImGui()` (descriptor pool, contexte, backends GLFW/Vulkan)
  - [x] 4.4 Implémenter `shutdown()` avec le code de `Application::shutdownImGui()`
  - [x] 4.5 Implémenter `beginFrame()`: NewFrame + renderMenuBar()
  - [x] 4.6 Implémenter `render(VkCommandBuffer)`: Render + RenderDrawData
  - [x] 4.7 Conservé `renderMenuBar()` dans ImGuiRenderer avec callback `m_onFileOpen` pour le menu Ouvrir

- [x] Task 5: Mettre à jour CMakeLists.txt (AC: #2)
  - [x] 5.1 Ajouté `src/UI/ImGuiRenderer.cpp` à VECNA_SOURCES

- [x] Task 6: Écrire les tests unitaires (AC: #1, #2, #3)
  - [x] 6.1 Test: IUIRenderer est abstract, non-copyable, non-movable, virtual destructor
  - [x] 6.2 Test: ImGuiRenderer hérite de IUIRenderer (std::is_base_of)
  - [x] 6.3 Test: Application.hpp compile sans dépendance ImGui (prouve l'isolation)

- [x] Task 7: Validation de la compilation et régression (AC: #1, #2, #3)
  - [x] 7.1 Compilation Debug réussie sans erreurs (vecna.exe + vecna_tests.exe)
  - [x] 7.2 80/80 tests passent (74 existants + 6 nouveaux UI) — zéro régression
  - [x] 7.3 Application démarre, ImGui initialized via abstraction, main loop fonctionne
  - [x] 7.4 Vérification manuelle par utilisateur recommandée: rotation trackball, zoom, pan, Ctrl+O

## Dev Notes

### Contexte architectural

L'architecture (architecture.md) définit un module UI avec abstraction interface-based:
- `include/Vecna/UI/IUIRenderer.hpp` - Interface abstraite
- `include/Vecna/UI/ImGuiRenderer.hpp` - Implémentation ImGUI
- `src/UI/ImGuiRenderer.cpp` - Code ImGUI
- Le module UI dépend de Core, Scene, Renderer [Source: architecture.md#Module Boundaries]

### État actuel du code ImGui

ImGui est actuellement intégré directement dans `Application.cpp` (Story 3-4) avec:
- `initImGui()` — crée descriptor pool, contexte ImGui, backends GLFW+Vulkan
- `shutdownImGui()` — détruit tout dans l'ordre inverse
- `renderImGui(VkCommandBuffer)` — NewFrame + menu bar + Render + RenderDrawData
- `renderMenuBar()` — menu Fichier avec Ouvrir/Quitter
- `handleKeyboardShortcuts()` — Ctrl+O (reste dans Application, pas lié à UI)
- Headers ImGui inclus dans Application.cpp: `<imgui.h>`, `<imgui_impl_glfw.h>`, `<imgui_impl_vulkan.h>`
- Descriptor pool: `VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT`, 10 per type, maxSets=100
- Callbacks GLFW installés AVANT ImGui (`install_callbacks=true` chaîne les callbacks)

### Paramètres nécessaires pour ImGuiRenderer::init()

Le constructeur/init d'ImGuiRenderer aura besoin de références à:
- `VulkanInstance` (pour `initInfo.Instance`)
- `VulkanDevice` (pour PhysicalDevice, Device, QueueFamily, Queue)
- `Swapchain` (pour ImageCount, RenderPass)
- `Window` (pour GLFW handle)

Option recommandée: passer ces paramètres au constructeur d'ImGuiRenderer, puis `Application` instancie `m_uiRenderer = std::make_unique<UI::ImGuiRenderer>(vulkanInstance, device, swapchain, window)`.

### Interactions callbacks GLFW

CRITIQUE: L'ordre d'installation des callbacks GLFW est important:
1. Application installe ses callbacks (trackball, pan, zoom)
2. ImGui chaîne sur ces callbacks (`install_callbacks=true`)
3. Chaque callback vérifie `ImGui::GetIO().WantCaptureMouse` pour ne pas interférer

Après refactoring, les callbacks restent dans Application.cpp. Les appels à `ImGui::GetIO().WantCaptureMouse` devront être abstraits ou conservés tels quels (acceptable pour MVP car ils utilisent l'API ImGui directement, mais c'est une dette technique connue).

### Conventions de code existantes

- Namespaces: `Vecna::Core`, `Vecna::Renderer`, `Vecna::Scene` → nouveau: `Vecna::UI`
- Nommage: PascalCase classes, camelCase méthodes, `m_` préfixe membres
- RAII: unique_ptr avec ordre de destruction explicite documenté en commentaires
- Logging: `Logger::info("UI", "message")` — le tag `[UI]` est déjà utilisé pour ImGui
- Headers d'agrégation: `Core.hpp`, `Renderer.hpp` → créer `UI.hpp`

### Dépendances CMake

ImGui est déjà intégré via FetchContent (v1.90.1) dans `cmake/FetchDependencies.cmake`. La target `imgui` est une STATIC library linkée publiquement avec glfw et Vulkan::Vulkan. Le nouveau `src/UI/ImGuiRenderer.cpp` sera compilé dans l'exécutable vecna qui linke déjà `imgui`.

### Gestion du WantCaptureMouse

Les callbacks dans Application.cpp utilisent `ImGui::GetIO().WantCaptureMouse` directement. Pour cette story (abstraction de base), on CONSERVE ces appels car:
1. Ils sont dans les callbacks GLFW de Application, pas dans le code UI
2. Les abstraire complètement nécessiterait une interface d'input events (hors scope)
3. C'est une dette technique acceptable documentée pour une future story

### Learnings des stories précédentes

- Story 3-4: ImGui descriptor pool réduit de 1000 à 10 par type lors du code review
- Story 3-4: ImGui non ré-initialisé lors de swapchain recreation (dette technique acceptée)
- Story 4-2: Bug glfwSetWindowUserPointer — Application est le seul owner
- Story 4-4: Les callbacks GLFW doivent respecter l'ordre d'installation pour le chaînage ImGui

### Project Structure Notes

- Nouveaux fichiers alignés avec l'architecture: `include/Vecna/UI/`, `src/UI/`
- Pas de conflit avec la structure existante
- Le module UI suit la même convention que Core et Renderer (header d'agrégation + sous-dossier)

### References

- [Source: architecture.md#Project Structure] — UI module layout
- [Source: architecture.md#Module Boundaries] — UI depends on Core, Scene, Renderer
- [Source: architecture.md#Architectural Decisions] — Interface-based UI abstraction
- [Source: epics.md#Story 5.1] — AC et user story
- [Source: 3-4-chargement-via-menu-fichier.md] — Code ImGui actuel dans Application
- [Source: Application.hpp:69-73] — Méthodes ImGui actuelles
- [Source: Application.cpp:391-506] — Implémentation ImGui actuelle

## Dev Agent Record

### Agent Model Used

Claude Opus 4.6

### Debug Log References

- Build error 1: `Logger::info` not found in `Vecna::UI` namespace — fixed by using `Core::Logger::info`
- Build error 2: test include path missing for `src/cgmath/StorageOrder.h` — added `${CMAKE_SOURCE_DIR}/..` to test includes

### Completion Notes List

- Created IUIRenderer abstract interface with init/shutdown/beginFrame/render/onSwapchainRecreated methods
- Created ImGuiRenderer concrete implementation, moved all ImGui code from Application.cpp
- Application now holds `unique_ptr<IUIRenderer>` — no ImGui types exposed in header
- `setOnFileOpen` callback lives on ImGuiRenderer (not IUIRenderer) — keeps interface clean for framework swaps
- Application sets callback on concrete ImGuiRenderer before upcasting to IUIRenderer
- Added `onSwapchainRecreated()` hook (default no-op) — called from drawFrame after swapchain recreation
- Application.cpp still includes `<imgui.h>` for `WantCaptureMouse` check in GLFW callbacks (documented technical debt)
- 6 new compile-time tests verify: abstract interface, virtual destructor, inheritance, no ImGui dependency in Application header, non-copyable/non-movable
- 80/80 tests pass, zero regressions
- Manual verification pending: runtime behavior (menu, file loading, camera navigation)

### Code Review Fixes (2026-02-27)

- [H1] Moved `setOnFileOpen`/`m_onFileOpen` from IUIRenderer to ImGuiRenderer — fixes leaky abstraction
- [L2] Removed `#include <functional>` from IUIRenderer.hpp (moved to ImGuiRenderer.hpp)
- [M1] Updated AC1 text to match actual interface methods
- [M2] Added `onSwapchainRecreated()` virtual method to IUIRenderer with default empty body; called from both swapchain recreation paths in drawFrame

### Change Log

- 2026-02-27: Story 5-1 implementation — UI abstraction layer created

### File List

- include/Vecna/UI/IUIRenderer.hpp (new) — Abstract UI renderer interface
- include/Vecna/UI/ImGuiRenderer.hpp (new) — ImGui concrete implementation header
- include/Vecna/UI.hpp (new) — UI module aggregation header
- src/UI/ImGuiRenderer.cpp (new) — ImGui implementation (moved from Application.cpp)
- include/Vecna/Core/Application.hpp (modified) — Replaced ImGui members with IUIRenderer
- src/Core/Application.cpp (modified) — Removed ImGui methods, uses IUIRenderer abstraction
- CMakeLists.txt (modified) — Added src/UI/ImGuiRenderer.cpp
- tests/UI/UIAbstractionTest.cpp (new) — 6 compile-time abstraction tests
- tests/CMakeLists.txt (modified) — Added UI test and include path

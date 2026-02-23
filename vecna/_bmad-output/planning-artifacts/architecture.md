---
stepsCompleted: [1, 2, 3, 4, 5, 6, 7, 8]
inputDocuments: ['prd.md']
workflowType: 'architecture'
project_name: 'vecna'
user_name: 'C_lau'
date: '2026-01-27'
lastStep: 8
status: 'complete'
completedAt: '2026-01-27'
---

# Architecture Decision Document

_This document builds collaboratively through step-by-step discovery. Sections are appended as we work through each architectural decision together._

## Project Context Analysis

### Requirements Overview

**Functional Requirements:**
22 FRs couvrant : gestion fichiers (OBJ/STL), rendu Vulkan, navigation caméra, informations modèle, interface utilisateur, et support multi-plateforme.

**Non-Functional Requirements:**
- Performance : 60+ FPS, support 1M+ triangles, chargement < 5s
- Reliability : Stabilité face aux erreurs fichiers et GPU
- Maintainability : Architecture modulaire pour extensibilité
- Usability : Interface intuitive sans documentation

**Scale & Complexity:**
- Primary domain: Desktop 3D Graphics (Vulkan)
- Complexity level: Medium-High
- Estimated architectural components: 6-8 modules

### Technical Constraints & Dependencies

- Vulkan API obligatoire (objectif d'apprentissage)
- Multi-plateforme : Windows, Linux, macOS (MoltenVK)
- Application 100% hors-ligne
- Build system : CMake
- Bibliothèque de fenêtrage cross-platform requise

### Cross-Cutting Concerns Identified

- Platform abstraction (windowing, input handling)
- GPU memory management
- Error handling and recovery
- 3D file format parsing
- Performance optimization

## Starter Template Evaluation

### Primary Technology Domain

Desktop 3D Graphics Application (Vulkan)

### Technical Preferences

| Component | Choice | Notes |
|-----------|--------|-------|
| Language | C++ | C++20 |
| Build System | CMake | Cross-platform |
| Windowing | GLFW | Léger, bien supporté Vulkan |
| Math Library | Custom | Pas de GLM |
| 3D Parsers | Custom | OBJ, STL implémentation maison |
| UI Framework | Dear ImGUI | Avec couche d'abstraction |

### Starter Approach: Minimal Custom Setup

**Rationale**: Maximum control and learning opportunity. No unnecessary dependencies.

**Base Dependencies**:
- Vulkan SDK
- GLFW (via CMake FetchContent ou submodule)
- Dear ImGUI (via CMake FetchContent ou submodule)

### Project Structure

```
vecna/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── core/           # Application core
│   ├── renderer/       # Vulkan rendering
│   ├── ui/             # UI abstraction + ImGUI impl
│   ├── scene/          # Model, camera, etc.
│   ├── loaders/        # OBJ, STL parsers
│   └── math/           # Custom math library
├── shaders/            # GLSL shaders
├── assets/             # Test models
└── third_party/        # GLFW, ImGUI
```

### Architectural Decisions from Setup

| Decision | Choice | Rationale |
|----------|--------|-----------|
| UI Abstraction | Interface-based | Allows future UI framework swap |
| Dependency Management | CMake FetchContent | Simple, no external package manager |
| Shader Compilation | glslc (offline) | Part of Vulkan SDK |

## Core Architectural Decisions

### Decision Priority Analysis

**Critical Decisions (Block Implementation):**
- C++20 standard
- VMA for GPU memory management
- Forward rendering pipeline
- Hybrid Vulkan code organization

**Important Decisions (Shape Architecture):**
- Interleaved vertex buffers
- Indexed mesh representation
- Return codes for error handling
- Monolithic shaders (MVP)

**Deferred Decisions (Post-MVP):**
- Deferred rendering (Phase 3 - multi-lights)
- Modular shader system (Phase 2 - render modes)

### Language & Build

| Decision | Choice | Rationale |
|----------|--------|-----------|
| C++ Standard | C++20 | Concepts, ranges, modern features |
| Build System | CMake | Cross-platform, industry standard |
| Compiler Support | GCC 10+, Clang 10+, MSVC 19.29+ | C++20 support |

### Rendering Architecture

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Rendering Pipeline | Forward | Simple, sufficient for single model viewer |
| Shader Organization | Monolithic | MVP simplicity, modular in Phase 2 |
| Shader Compilation | Offline (glslc) | Part of Vulkan SDK |

### Memory & Data

| Decision | Choice | Rationale |
|----------|--------|-----------|
| GPU Memory | VMA (Vulkan Memory Allocator) | Industry standard, simplifies allocation |
| Vertex Layout | Interleaved | Better cache locality, performance |
| Mesh Storage | Indexed | Memory efficient, natural for OBJ/STL |

### Code Organization

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Vulkan Wrappers | Hybrid | Classes for main objects, functions for utilities |
| Error Handling | Return codes | Explicit, compatible with VkResult |
| UI Abstraction | Interface-based | Allows future UI framework swap |

### Decision Impact Analysis

**Implementation Sequence:**
1. CMake setup + dependencies (GLFW, VMA, ImGUI)
2. Vulkan initialization (Instance, Device, Swapchain wrappers)
3. Rendering pipeline (Forward renderer)
4. Mesh loading (OBJ/STL parsers → indexed mesh)
5. Camera system (Trackball navigation)
6. UI integration (ImGUI with abstraction layer)
7. Info panel (Bounding box, stats display)

**Cross-Component Dependencies:**
- VMA requires Vulkan device → init order matters
- ImGUI requires Vulkan context → init after swapchain
- Mesh loading independent → can develop in parallel
- Camera affects MVP matrix → renderer consumes camera output

## Implementation Patterns & Consistency Rules

### Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Classes/Structs | PascalCase | `VulkanDevice`, `MeshLoader` |
| Functions/Methods | camelCase | `createBuffer()`, `loadModel()` |
| Private Members | m_ prefix | `m_device`, `m_buffer` |
| Constants | UPPER_SNAKE_CASE | `MAX_FRAMES_IN_FLIGHT` |
| Files | PascalCase | `VulkanDevice.cpp`, `MeshLoader.hpp` |
| Namespaces | PascalCase | `Vecna::Renderer` |

### File Organization

| Aspect | Pattern |
|--------|---------|
| Headers | `include/Vecna/<Module>/` |
| Sources | `src/<Module>/` |
| Tests | `tests/<Module>/` |
| Module Headers | Aggregation header per module |

**Example Structure:**
```
include/
└── Vecna/
    ├── Renderer/
    │   ├── VulkanDevice.hpp
    │   ├── Swapchain.hpp
    │   └── Pipeline.hpp
    ├── Renderer.hpp          # Aggregation header
    └── Vecna.hpp             # Main header
src/
└── Renderer/
    ├── VulkanDevice.cpp
    ├── Swapchain.cpp
    └── Pipeline.cpp
```

### Code Patterns

**Resource Management:**
- RAII mandatory for all Vulkan resources
- Destructor releases resources, no manual cleanup required

**Ownership:**
- `std::unique_ptr` by default
- `std::shared_ptr` only when ownership is genuinely shared
- Raw pointers only for non-owning references

**Parameters & Returns:**
- `const T&` for read-only input
- Prefer `return` over output parameters
- Use `std::optional<T>` for nullable returns
- Use structured bindings / tuples for multiple returns

**Headers:**
- `#pragma once` for all headers

### Error Handling

**Dual Strategy:**
- **Exceptions** pour les erreurs fatales/irrécupérables (initialisation Vulkan, GPU manquant)
- **Return codes** pour les erreurs récupérables (chargement fichier, parsing)

**Exceptions (Fatal Errors):**
```cpp
// Pour les erreurs d'initialisation irrécupérables
throw std::runtime_error("Failed to create Vulkan instance");
```

**Error Codes (Recoverable Errors):**
- Enum class per module
- Example: `Vecna::Loader::Error::FileNotFound`

```cpp
namespace Vecna::Loader {
    enum class Error {
        None = 0,
        FileNotFound,
        InvalidFormat,
        ParseError
    };
}
```

**Return Pattern:**
```cpp
struct LoadResult {
    Loader::Error error;
    std::optional<Mesh> mesh;
};

LoadResult loadOBJ(const std::filesystem::path& path);
```

### Logging

**Levels:** DEBUG, INFO, WARN, ERROR

**Format:** `[MODULE] Message`

**Examples:**
```
[Renderer] Vulkan instance created
[Loader] Loading model: cube.obj
[Loader] ERROR: File not found: missing.obj
[UI] ImGUI initialized
```

### Enforcement Guidelines

**All code MUST:**
- Follow naming conventions exactly
- Use RAII for Vulkan resources
- Use `#pragma once` in all headers
- Use exceptions for fatal/unrecoverable errors (Vulkan init, missing GPU)
- Use return codes for recoverable errors (file loading, parsing)
- Log with module prefix and appropriate level

**Anti-Patterns to Avoid:**
- Manual resource cleanup (use RAII)
- Output parameters (use return values)
- Raw owning pointers (use smart pointers)
- Global error state (use return codes)

## Project Structure & Boundaries

### Complete Project Directory Structure

```
vecna/
├── CMakeLists.txt                    # Root CMake configuration
├── README.md
├── .gitignore
├── .clang-format                     # Code formatting rules
├── .github/
│   └── workflows/
│       └── ci.yml                    # CI/CD pipeline
│
├── cmake/
│   ├── CompilerWarnings.cmake        # Warning flags
│   ├── FetchDependencies.cmake       # GLFW, VMA, ImGUI
│   └── ShaderCompilation.cmake       # glslc integration
│
├── include/
│   └── Vecna/
│       ├── Vecna.hpp                 # Main aggregation header
│       │
│       ├── Core/
│       │   ├── Application.hpp
│       │   ├── Window.hpp
│       │   ├── Input.hpp
│       │   └── Logger.hpp
│       ├── Core.hpp                  # Core aggregation
│       │
│       ├── Renderer/
│       │   ├── VulkanInstance.hpp
│       │   ├── VulkanDevice.hpp
│       │   ├── Swapchain.hpp
│       │   ├── Pipeline.hpp
│       │   ├── Buffer.hpp
│       │   ├── CommandPool.hpp
│       │   └── Renderer.hpp          # Facade
│       ├── Renderer.hpp              # Renderer aggregation
│       │
│       ├── Scene/
│       │   ├── Mesh.hpp
│       │   ├── Model.hpp
│       │   ├── Camera.hpp
│       │   ├── Trackball.hpp
│       │   └── BoundingBox.hpp
│       ├── Scene.hpp                 # Scene aggregation
│       │
│       ├── Loaders/
│       │   ├── OBJLoader.hpp
│       │   ├── STLLoader.hpp
│       │   └── LoaderErrors.hpp
│       ├── Loaders.hpp               # Loaders aggregation
│       │
│       ├── UI/
│       │   ├── IUIRenderer.hpp       # Abstract interface
│       │   ├── ImGuiRenderer.hpp     # ImGUI implementation
│       │   └── InfoPanel.hpp
│       ├── UI.hpp                    # UI aggregation
│       │
│       └── Math/
│           ├── Vec3.hpp
│           ├── Vec4.hpp
│           ├── Mat4.hpp
│           ├── Quaternion.hpp
│           └── MathUtils.hpp
│
├── src/
│   ├── main.cpp                      # Entry point
│   │
│   ├── Core/
│   │   ├── Application.cpp
│   │   ├── Window.cpp
│   │   ├── Input.cpp
│   │   └── Logger.cpp
│   │
│   ├── Renderer/
│   │   ├── VulkanInstance.cpp
│   │   ├── VulkanDevice.cpp
│   │   ├── Swapchain.cpp
│   │   ├── Pipeline.cpp
│   │   ├── Buffer.cpp
│   │   ├── CommandPool.cpp
│   │   └── Renderer.cpp
│   │
│   ├── Scene/
│   │   ├── Mesh.cpp
│   │   ├── Model.cpp
│   │   ├── Camera.cpp
│   │   ├── Trackball.cpp
│   │   └── BoundingBox.cpp
│   │
│   ├── Loaders/
│   │   ├── OBJLoader.cpp
│   │   └── STLLoader.cpp
│   │
│   └── UI/
│       ├── ImGuiRenderer.cpp
│       └── InfoPanel.cpp
│
├── shaders/
│   ├── CMakeLists.txt                # Shader compilation
│   ├── basic.vert
│   ├── basic.frag
│   └── compiled/                     # SPIR-V output (generated)
│
├── tests/
│   ├── CMakeLists.txt
│   ├── Core/
│   │   └── LoggerTest.cpp
│   ├── Loaders/
│   │   ├── OBJLoaderTest.cpp
│   │   └── STLLoaderTest.cpp
│   ├── Scene/
│   │   └── BoundingBoxTest.cpp
│   └── Math/
│       ├── Vec3Test.cpp
│       └── Mat4Test.cpp
│
├── assets/                           # Test models
│   ├── cube.obj
│   ├── sphere.stl
│   └── README.md
│
└── third_party/                      # External dependencies
    └── CMakeLists.txt                # FetchContent config
```

### Architectural Boundaries

**Module Boundaries:**

| Module | Depends On | Exposes |
|--------|------------|---------|
| Core | - | Window, Input, Logger |
| Math | - | Vec3, Mat4, Quaternion |
| Loaders | Math | OBJLoader, STLLoader, Mesh data |
| Scene | Math | Model, Camera, BoundingBox |
| Renderer | Core, Scene, Math | Renderer facade |
| UI | Core, Scene, Renderer | IUIRenderer, InfoPanel |

**Dependency Graph:**
```
        ┌─────────┐
        │   UI    │
        └────┬────┘
             │
    ┌────────┼────────┐
    ▼        ▼        ▼
┌────────┐ ┌──────┐ ┌────────┐
│Renderer│ │Scene │ │Loaders │
└───┬────┘ └──┬───┘ └───┬────┘
    │         │         │
    └─────────┼─────────┘
              ▼
          ┌──────┐
          │ Core │
          └──┬───┘
             ▼
          ┌──────┐
          │ Math │
          └──────┘
```

### Data Flow

```
File (OBJ/STL)
     │
     ▼
┌─────────┐      ┌───────┐
│ Loaders │ ───▶ │ Mesh  │
└─────────┘      └───┬───┘
                     │
                     ▼
                ┌─────────┐
                │  Model  │◀─── BoundingBox
                └────┬────┘
                     │
         ┌───────────┼───────────┐
         ▼           ▼           ▼
    ┌────────┐  ┌────────┐  ┌────────┐
    │ Camera │  │Renderer│  │   UI   │
    └────────┘  └────────┘  └────────┘
```

### Requirements to Structure Mapping

| FR Category | Module | Key Files |
|-------------|--------|-----------|
| FR1-FR6 (Files) | Loaders | OBJLoader, STLLoader |
| FR7-FR9 (Rendering) | Renderer | VulkanDevice, Pipeline, Renderer |
| FR10-FR12 (Navigation) | Scene | Camera, Trackball |
| FR13-FR16 (Model Info) | Scene | Model, BoundingBox, Mesh |
| FR17-FR19 (UI) | UI | IUIRenderer, InfoPanel |
| FR20-FR22 (Platform) | Core, cmake | Window, CMakeLists |

### Integration Points

| Point | From | To | Method |
|-------|------|-----|--------|
| File Load | UI | Loaders | Direct call |
| Mesh Data | Loaders | Scene | Return value |
| Model Stats | Scene | UI | Getter methods |
| MVP Matrix | Camera | Renderer | Uniform buffer |
| Input Events | Core | Camera | Callbacks |
| Render Loop | Core | Renderer, UI | Main loop |

## Architecture Validation Results

### Coherence Validation ✅

**Decision Compatibility:**
- C++20, VMA, GLFW, et Dear ImGUI sont pleinement compatibles
- CMake FetchContent supporte toutes les dépendances externes
- Forward rendering approprié pour un viewer single-model
- Vulkan Memory Allocator compatible avec le pipeline Vulkan standard

**Pattern Consistency:**
- Conventions de nommage appliquées uniformément (PascalCase classes, camelCase fonctions, m_ membres)
- RAII obligatoire pour toutes les ressources Vulkan
- Return codes cohérents avec VkResult de Vulkan
- Smart pointers et ownership clairement définis

**Structure Alignment:**
- Séparation include/src supporte les headers d'agrégation
- Modules indépendants avec dépendances unidirectionnelles
- Tests isolés par module

### Requirements Coverage Validation ✅

**Functional Requirements Coverage:**

| FR Category | Module | Status |
|-------------|--------|--------|
| FR1-FR6 (Files) | Loaders | ✅ OBJLoader, STLLoader, LoaderErrors |
| FR7-FR9 (Rendering) | Renderer | ✅ VulkanDevice, Pipeline, Renderer |
| FR10-FR12 (Navigation) | Scene | ✅ Camera, Trackball |
| FR13-FR16 (Model Info) | Scene | ✅ Model, BoundingBox, Mesh |
| FR17-FR19 (UI) | UI | ✅ IUIRenderer, InfoPanel |
| FR20-FR22 (Platform) | Core, cmake | ✅ Window, CMakeLists |

**Non-Functional Requirements Coverage:**

| NFR | Architectural Support |
|-----|----------------------|
| NFR1-NFR4 (Performance) | VMA, interleaved vertices, indexed mesh, forward rendering |
| NFR5-NFR7 (Reliability) | Return codes, RAII, error enums per module |
| NFR8-NFR10 (Maintainability) | Modular structure, clear boundaries, CMake cross-platform |
| NFR11-NFR12 (Usability) | ImGUI via abstraction, logging with levels |

### Implementation Readiness Validation ✅

**Decision Completeness:**
- Toutes les décisions critiques documentées avec versions (C++20, GCC 10+/Clang 10+/MSVC 19.29+)
- Patterns d'implémentation avec exemples de code
- Règles de consistance explicites et applicables

**Structure Completeness:**
- Arborescence complète avec tous fichiers et répertoires
- Points d'intégration clairement spécifiés
- Frontières de composants bien définies

**Pattern Completeness:**
- Conventions de nommage complètes
- Patterns de communication (callbacks, return values)
- Patterns de processus (error handling, logging) documentés

### Gap Analysis Results

**Aucun gap critique identifié.**

**Gaps importants (non-bloquants):**
- Versioning des dépendances externes non spécifié (VMA version, GLFW version) - peut être défini lors du setup CMake

**Nice-to-have:**
- Documentation de l'API publique (génération Doxygen)
- Guidelines de contribution
- Script de setup dev environment

### Architecture Completeness Checklist

**✅ Requirements Analysis**
- [x] Contexte projet analysé (22 FRs, 12 NFRs)
- [x] Complexité évaluée (Medium-High)
- [x] Contraintes techniques identifiées (Vulkan obligatoire, cross-platform)
- [x] Concerns transversaux mappés (platform abstraction, GPU memory, error handling)

**✅ Architectural Decisions**
- [x] Décisions critiques documentées (C++20, VMA, Forward rendering)
- [x] Stack technique complet (GLFW, VMA, ImGUI, CMake)
- [x] Patterns d'intégration définis (callbacks, return values)
- [x] Considérations performance adressées (interleaved, indexed)

**✅ Implementation Patterns**
- [x] Conventions de nommage établies
- [x] Patterns de structure définis
- [x] Patterns de communication spécifiés
- [x] Patterns de processus documentés (errors, logging)

**✅ Project Structure**
- [x] Arborescence complète définie
- [x] Frontières de composants établies
- [x] Points d'intégration mappés
- [x] Mapping requirements → structure complet

### Architecture Readiness Assessment

**Overall Status:** READY FOR IMPLEMENTATION

**Confidence Level:** High

**Key Strengths:**
- Architecture modulaire claire avec dépendances unidirectionnelles
- Décisions optimisées pour l'apprentissage Vulkan progressif
- UI abstraite permettant l'évolution future
- Patterns explicites prévenant les conflits d'implémentation

**Areas for Future Enhancement:**
- Phase 2: Shader modulaire pour modes de rendu alternatifs
- Phase 3: Deferred rendering pour multi-lights
- Documentation API avec Doxygen

### Implementation Handoff

**AI Agent Guidelines:**
- Suivre toutes les décisions architecturales exactement comme documentées
- Utiliser les patterns d'implémentation de façon consistante
- Respecter la structure projet et les frontières de modules
- Référer ce document pour toutes questions architecturales

**First Implementation Priority:**
1. CMake setup avec FetchContent (GLFW, VMA, ImGUI)
2. Vulkan initialization (Instance, Device, Swapchain)
3. Basic rendering pipeline
4. Mesh loading (OBJ/STL parsers)

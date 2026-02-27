# Story 4.3: Zoom Molette

Status: done

## Story

As a utilisateur,
I want zoomer avec la molette de la souris,
so that je puisse voir les détails ou la vue d'ensemble.

## Acceptance Criteria

### AC1: Zoom In
**Given** un modèle est affiché
**When** je scroll vers le haut (molette avant)
**Then** la caméra se rapproche du modèle (zoom in)
**And** le mouvement est progressif et fluide

### AC2: Zoom Out
**Given** un modèle est affiché
**When** je scroll vers le bas (molette arrière)
**Then** la caméra s'éloigne du modèle (zoom out)
**And** le mouvement est progressif et fluide

### AC3: Limite Zoom In
**Given** je zoome très près
**When** la distance minimale est atteinte
**Then** le zoom s'arrête à la limite
**And** la caméra ne traverse pas le modèle

### AC4: Limite Zoom Out
**Given** je zoome très loin
**When** la distance maximale est atteinte
**Then** le zoom s'arrête à la limite
**And** le modèle reste visible à l'écran

## Tasks / Subtasks

- [x] Task 1: Ajouter le callback scroll GLFW (AC: #1, #2)
  - [x] Ajouter déclaration static scrollCallback dans Application.hpp
  - [x] Enregistrer glfwSetScrollCallback dans le constructeur (avant initImGui)
  - [x] Vérifier ImGui::WantCaptureMouse pour ne pas interférer avec les menus

- [x] Task 2: Implémenter le zoom multiplicatif (AC: #1, #2, #3, #4)
  - [x] Utiliser m_cameraDistance (membre autoritatif) au lieu de pos[2]
  - [x] Appliquer un facteur multiplicatif (1 - yoffset * ZOOM_FACTOR)
  - [x] Clamper le facteur scale à [0.1, 10.0] (protection molettes haute résolution)
  - [x] Clamper la distance à [MIN_DISTANCE, MAX_DISTANCE] via std::clamp
  - [x] Mettre à jour les clipping planes proportionnellement
  - [x] Initialiser les clipping planes dans le constructeur (cohérence avec distance par défaut Z=3)

- [x] Task 3: Constantes nommées (AC: #3, #4)
  - [x] ZOOM_FACTOR = 0.1 (10% par cran de molette)
  - [x] MIN_DISTANCE = 0.01
  - [x] MAX_DISTANCE = 10000.0

## Dev Notes

### Approche zoom multiplicatif

Le zoom utilise un membre autoritatif `m_cameraDistance` plutôt que d'extraire la position Z de la caméra. Cela évite les problèmes si la caméra est déplacée hors de l'axe Z (futur pan story 4-4).

Le facteur scale est clampé à `[0.1, 10.0]` pour protéger contre les molettes haute résolution (touchpads, souris gaming) qui peuvent reporter `yoffset > 10`, ce qui rendrait le facteur négatif.

La distance est ensuite clampée à `[MIN_DISTANCE, MAX_DISTANCE]` via `std::clamp`. Les clipping planes sont recalculés à chaque zoom (`near = distance * 0.01`, `far = distance * 10`) et aussi initialisés dans le constructeur pour cohérence avec la distance caméra par défaut (Z=3).

### Pattern callback identique à Story 4-2

Le scroll callback suit exactement le même pattern que les callbacks trackball :
- Statique, routé via `glfwGetWindowUserPointer` → `Application*`
- Vérifie `ImGui::GetIO().WantCaptureMouse` avant traitement
- Installé avant `initImGui()` pour le chaînage ImGui

### References

- [Source: architecture.md#FR11] — zoom molette souris
- [Source: 4-2-rotation-trackball.md] — pattern callbacks GLFW

## Dev Agent Record

### Agent Model Used

claude-opus-4-6

### Completion Notes List

- Scroll callback ajouté (glfwSetScrollCallback) dans Application
- Zoom multiplicatif avec facteur 10% par cran
- m_cameraDistance membre autoritatif (ne dépend plus de pos[2])
- Facteur scale clampé à [0.1, 10.0] (protection molettes haute résolution)
- Distance caméra clampée à [0.01, 10000] via std::clamp
- Clipping planes recalculés dynamiquement (near=0.01×dist, far=10×dist)
- Clipping planes initialisés dans le constructeur (cohérence Z=3 par défaut)
- loadModel met à jour m_cameraDistance
- ImGui::WantCaptureMouse vérifié
- Build successful (Debug)

### File List

- include/Vecna/Core/Application.hpp (modified — added scrollCallback declaration, m_cameraDistance member)
- src/Core/Application.cpp (modified — scroll callback, zoom constants, zoom via m_cameraDistance, initial clipping planes, loadModel updates m_cameraDistance)

## Change Log

- 2026-02-26: Story implemented — scroll wheel zoom with multiplicative factor and distance clamping
- 2026-02-26: Code review fixes — m_cameraDistance member, scale clamping, initial clipping planes, std::clamp

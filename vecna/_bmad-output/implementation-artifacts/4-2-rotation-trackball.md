# Story 4.2: Rotation Trackball

Status: done

## Story

As a utilisateur,
I want faire tourner le modèle 3D avec la souris (clic gauche + drag),
so that je peux examiner le modèle sous tous les angles de manière interactive.

## Acceptance Criteria

### AC1: Trackball Rotation
**Given** l'application affiche un modèle 3D
**When** l'utilisateur fait clic gauche + drag horizontal
**Then** le modèle tourne autour de l'axe Y

### AC2: Combined Rotation
**Given** l'application affiche un modèle 3D
**When** l'utilisateur fait clic gauche + drag diagonal
**Then** le modèle tourne de manière combinée fluide (trackball)

### AC3: Rotation Stability
**Given** l'utilisateur a effectué une rotation
**When** le bouton souris est relâché
**Then** la rotation reste stable sans drift

### AC4: ImGui Compatibility
**Given** le menu Fichier est affiché
**When** l'utilisateur interagit avec le menu
**Then** le trackball n'interfère pas (ImGui::WantCaptureMouse vérifié)

### AC5: Model Loading Compatibility
**Given** un modèle est chargé via Fichier > Ouvrir
**When** l'utilisateur fait clic gauche + drag
**Then** le trackball fonctionne sur le nouveau modèle

## Tasks / Subtasks

- [x] Task 1: Créer la classe Trackball dans Scene (AC: #1, #2, #3)
  - [x] Créer include/Vecna/Scene/Trackball.hpp
  - [x] Créer src/Scene/Trackball.cpp
  - [x] Porter l'algorithme de projection hémisphérique (tbPointToVector) depuis cgre
  - [x] Implémenter Rodrigues rotation formula (remplace GL matrix stack)
  - [x] Accumuler les rotations dans m_transform (matrice 4x4 column-major)

- [x] Task 2: Intégrer le trackball dans Application (AC: #1, #2, #3, #4, #5)
  - [x] Ajouter forward-declaration Scene::Trackball dans Application.hpp
  - [x] Ajouter membre m_trackball (unique_ptr)
  - [x] Ajouter callbacks GLFW statiques (mouseButtonCallback, cursorPosCallback)
  - [x] Supprimer m_rotationAngle et ROTATION_SPEED
  - [x] Enregistrer callbacks AVANT initImGui (chaînage ImGui)
  - [x] Vérifier ImGui::GetIO().WantCaptureMouse dans les callbacks
  - [x] Copier m_trackball->getTransform() dans la matrice model (recordCommandBuffer)
  - [x] Appeler setDimensions() lors du recreate swapchain

- [x] Task 3: Ajouter le source au build (AC: #1)
  - [x] Ajouter src/Scene/Trackball.cpp dans CMakeLists.txt

## Dev Notes

### Algorithme Trackball (porté de cgre/examinator_trackball.cpp)

L'algorithme projette la position souris (x,y) sur une hémisphère unitaire via `pointToVector()`:
- Normalise x,y dans [-1, 1] relatif à la fenêtre
- Calcule z = cos(π/2 * d) où d = distance au centre
- Normalise le vecteur résultant

La rotation est calculée entre deux positions successives:
- Angle = 180° × ‖delta‖
- Axe = produit vectoriel (currentPosition × lastPosition) — inversé par rapport à cgre pour correspondre aux conventions Vulkan/column-major

**Remplacement du GL matrix stack**: le code cgre utilisait `glRotatef`/`glMultMatrixf`/`glGetFloatv` pour accumuler les rotations. Remplacé par la formule de Rodrigues:
```
R = I·cos(θ) + (1-cos(θ))·(axis⊗axis) + sin(θ)·[axis]×
m_transform = m_transform × R  (post-multiplication)
```
Post-multiplication : la rotation incrémentale est appliquée dans le repère local de l'objet, ce qui garantit un comportement intuitif (le drag suit le curseur) indépendamment de la rotation accumulée.

**Stockage column-major**: `m_transform[col][row]` — layout mémoire identique à `TMatrix4<ColumnMajor>`, compatible directement avec les push constants Vulkan via `std::copy`.

### Callbacks GLFW unifiés

Application est l'unique propriétaire du `glfwSetWindowUserPointer` et installe tous les callbacks GLFW (resize, mouse button, cursor position). Le resize est routé vers `Window::onFramebufferResize()`. Les callbacks trackball sont installés AVANT `initImGui()` pour le chaînage ImGui :
1. GLFW déclenche le callback ImGui
2. ImGui traite l'événement, met à jour WantCaptureMouse
3. ImGui appelle notre callback (chaîné comme "previous")
4. Notre callback vérifie WantCaptureMouse et ignore si ImGui veut la souris

### References

- [Source: cgre/examinator_trackball.cpp] — algorithme original
- [Source: architecture.md#Scene Module] — placement dans le module Scene
- [Source: 2-4-cube-3d-avec-depth-buffer.md] — matrices MVP existantes

## Dev Agent Record

### Agent Model Used

claude-opus-4-6

### Completion Notes List

- Classe Trackball créée dans Vecna::Scene (include/Vecna/Scene/Trackball.hpp, src/Scene/Trackball.cpp)
- Algorithme cgre porté en pur C++ (pas de dépendance OpenGL)
- Rodrigues rotation formula remplace le GL matrix stack
- Stockage column-major `m_transform[col][row]` compatible directement avec TMatrix4 ColumnMajor
- Post-multiplication `m_transform × R` pour rotation intuitive indépendante de la rotation accumulée
- Auto-rotation supprimée (m_rotationAngle, ROTATION_SPEED)
- Application est l'unique propriétaire du glfwSetWindowUserPointer (résout conflit avec Window)
- Callbacks GLFW (resize, mouse) unifiés dans Application, resize routé vers Window::onFramebufferResize()
- ImGui::WantCaptureMouse vérifié pour ne pas interférer avec les menus
- Guard division-by-zero dans pointToVector (m_width/m_height <= 0)
- Build successful (Debug)

### File List

- include/Vecna/Scene/Trackball.hpp (new)
- src/Scene/Trackball.cpp (new)
- include/Vecna/Core/Application.hpp (modified — added Trackball member, GLFW callbacks, removed m_rotationAngle)
- src/Core/Application.cpp (modified — trackball integration, unified GLFW callbacks, removed auto-rotation)
- include/Vecna/Core/Window.hpp (modified — replaced private static callback with public onFramebufferResize)
- src/Core/Window.cpp (modified — removed glfwSetWindowUserPointer and glfwSetFramebufferSizeCallback)
- CMakeLists.txt (modified — added src/Scene/Trackball.cpp)

## Change Log

- 2026-02-26: Story implemented — trackball rotation replaces auto-rotation
- 2026-02-26: Code review fixes — HIGH-1 glfwSetWindowUserPointer conflict, MED-1 div-by-zero guard, MED-2 column-major storage, MED-3 docs updated, LOW-1 named PI constant

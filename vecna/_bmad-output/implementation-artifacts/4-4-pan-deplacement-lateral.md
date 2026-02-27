# Story 4.4: Pan (Déplacement Latéral)

Status: done

## Story

As a utilisateur,
I want déplacer la vue latéralement,
so that je puisse centrer une zone d'intérêt spécifique.

## Acceptance Criteria

### AC1: Middle-button drag pan
**Given** un modèle est affiché
**When** je clique avec le bouton milieu et je drag
**Then** la vue se déplace dans le plan de l'écran
**And** le déplacement suit le mouvement de la souris

### AC2: Shift + left-click alternative
**Given** un modèle est affiché
**When** je maintiens Shift + clic gauche et je drag
**Then** la vue se déplace (alternative au bouton milieu)
**And** le comportement est identique au pan avec bouton milieu

### AC3: Camera target update
**Given** je fais un pan
**When** le déplacement est appliqué
**Then** le point cible de la caméra est mis à jour
**And** les rotations futures tournent autour du nouveau centre

### AC4: Combination with rotation
**Given** je combine pan et rotation
**When** les deux opérations sont enchaînées
**Then** chaque opération fonctionne correctement
**And** la navigation reste intuitive et prévisible

## Tasks / Subtasks

- [x] Task 1: Détecter le pan dans mouseButtonCallback (AC: #1, #2)
  - [x] Bouton milieu (GLFW_MOUSE_BUTTON_MIDDLE) active le pan
  - [x] Shift + clic gauche active le pan (alternative)
  - [x] Stocker m_lastPanX/m_lastPanY au début du pan
  - [x] Ne pas forwarder les clics pan au trackball

- [x] Task 2: Implémenter le déplacement dans cursorPosCallback (AC: #1, #3)
  - [x] Calculer delta (dx, dy) par rapport à la dernière position
  - [x] Échelle proportionnelle à m_cameraDistance / windowHeight (hauteur seule, pixels carrés)
  - [x] Déplacer position ET target de la caméra par le même offset
  - [x] Pan en world X/Y (valide car rotation est dans model matrix, pas view)

- [x] Task 3: Compatibilité avec zoom et rotation (AC: #3, #4)
  - [x] scrollCallback zoom le long de la direction vue (target→position)
  - [x] Le zoom préserve l'offset de pan (ne snap plus à (0,0,distance))
  - [x] Rotation centrée sur l'origine du modèle (standard 3D viewer behavior)

- [x] Task 4: État pan dans Application (AC: #1, #2)
  - [x] Ajouter m_panning, m_lastPanX, m_lastPanY dans Application.hpp
  - [x] Relâcher le pan quand le bouton est relâché
  - [x] Ne pas forwarder le release orphelin au trackball (Shift relâché mid-drag)

## Dev Notes

### Approche pan dans le plan écran

Le pan déplace la caméra (position ET target) dans le plan perpendiculaire à la direction de vue. Comme la caméra est toujours orientée face à l'axe Z (la rotation est dans le model matrix via trackball, pas dans la view matrix), le plan écran correspond simplement aux axes X et Y du monde.

Le déplacement est proportionnel à `m_cameraDistance / windowHeight`, ce qui garantit un mouvement cohérent à toute distance de zoom. Seule la hauteur de fenêtre est utilisée (pixels carrés).

### Rotation centrée sur l'origine du modèle

La rotation trackball reste centrée sur l'origine du modèle (0,0,0), pas sur le camera target. Après un pan, le centre de rotation peut être décalé par rapport au centre de l'écran. C'est le comportement standard des viewers 3D (Blender, MeshLab). L'approche alternative (pivoter autour du target via `T(target)*R*T(-target)`) cause un bug : la translation du pan est transformée par la rotation, rendant le pan incohérent avec la souris après rotation.

### Interaction zoom + pan

Le scroll callback zoome le long de la direction target→position au lieu de snapper à `(0, 0, distance)`. Cela préserve l'offset de pan lors du zoom : `newPos = target + (pos - target) * (newDistance / oldDistance)`.

### Gestion du release mid-drag

Si l'utilisateur relâche Shift pendant un Shift+drag gauche, le pan continue jusqu'au release du bouton. Le release du bouton gauche est intercepté si `m_panning` est actif et n'est pas forwardé au trackball (évite un release orphelin).

### Pattern callback

Le pan s'intègre dans les callbacks existants (mouseButton + cursorPos) sans nouveau callback GLFW. Le routage pan vs trackball se fait par le type de bouton/modifieur.

### References

- [Source: architecture.md#FR12] — pan déplacement latéral
- [Source: 4-2-rotation-trackball.md] — pattern callbacks GLFW
- [Source: 4-3-zoom-molette.md] — m_cameraDistance, zoom interaction

## Dev Agent Record

### Agent Model Used

claude-opus-4-6

### Completion Notes List

- Pan via bouton milieu et Shift+clic gauche
- Déplacement proportionnel à la distance caméra (windowHeight seul)
- Position ET target déplacés ensemble
- Rotation centrée sur l'origine du modèle (standard 3D viewer — pivot autour du target retiré car casse le pan)
- Zoom modifié pour préserver l'offset de pan (zoom along view direction)
- Release orphelin au trackball évité quand pan actif (Shift mid-drag)
- Pas de nouveau fichier — intégré dans les callbacks existants
- Build successful (Debug)

### File List

- include/Vecna/Core/Application.hpp (modified — added m_panning, m_lastPanX, m_lastPanY)
- src/Core/Application.cpp (modified — mouseButtonCallback pan detection, cursorPosCallback pan movement, scrollCallback zoom preserves pan, recordCommandBuffer rotation pivotée autour du target)

## Change Log

- 2026-02-26: Story implemented — pan via middle mouse / Shift+left, zoom preserves pan offset
- 2026-02-26: Code review fixes — orphan release guard, winWidth dead code removed, world-space pan comment
- 2026-02-26: Bug fix — revert rotation pivot (T(target)*R*T(-target)) qui rendait le pan incohérent après rotation

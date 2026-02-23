---
stepsCompleted: ['step-01-validate-prerequisites', 'step-02-design-epics', 'step-03-create-stories', 'step-04-final-validation']
inputDocuments: ['prd.md', 'architecture.md']
workflowType: 'epics'
project_name: 'vecna'
user_name: 'C_lau'
date: '2026-01-27'
status: 'complete'
completedAt: '2026-01-27'
---

# Vecna - Epic Breakdown

## Overview

This document provides the complete epic and story breakdown for Vecna, decomposing the requirements from the PRD, UX Design if it exists, and Architecture requirements into implementable stories.

## Requirements Inventory

### Functional Requirements

**File Management:**
- FR1: L'utilisateur peut ouvrir un fichier 3D via le menu Fichier
- FR2: L'utilisateur peut ouvrir un fichier 3D par drag & drop
- FR3: L'utilisateur peut charger des fichiers au format OBJ
- FR4: L'utilisateur peut charger des fichiers au format STL
- FR5: Le système détecte et signale les fichiers corrompus ou invalides
- FR6: Le système retourne à un état stable après une erreur de chargement

**3D Rendering:**
- FR7: Le système affiche le modèle 3D via l'API Vulkan
- FR8: Le système centre automatiquement le modèle après chargement
- FR9: Le système affiche visuellement la bounding box du modèle

**Camera Navigation:**
- FR10: L'utilisateur peut effectuer une rotation trackball (souris)
- FR11: L'utilisateur peut zoomer (molette souris)
- FR12: L'utilisateur peut effectuer un pan (déplacement latéral)

**Model Information:**
- FR13: L'utilisateur peut consulter les dimensions de la bounding box (X, Y, Z)
- FR14: L'utilisateur peut consulter la position du modèle (origine, centre)
- FR15: L'utilisateur peut consulter le nombre de faces
- FR16: L'utilisateur peut consulter le nombre de sommets

**User Interface:**
- FR17: Le système affiche les informations du modèle dans un panneau dédié
- FR18: Le système affiche des messages d'erreur clairs
- FR19: L'utilisateur peut quitter l'application proprement

**Cross-Platform:**
- FR20: L'application fonctionne sur Windows
- FR21: L'application fonctionne sur Linux
- FR22: L'application fonctionne sur macOS

### NonFunctional Requirements

**Performance:**
- NFR1: 60+ FPS avec modèles jusqu'à 1M triangles
- NFR2: Chargement fichier 1M triangles < 5 secondes
- NFR3: Navigation fluide sans latence perceptible
- NFR4: Mise à jour informations < 100ms

**Reliability:**
- NFR5: Pas de crash sur fichiers invalides
- NFR6: Récupération gracieuse des erreurs GPU/Vulkan
- NFR7: État cohérent après erreur

**Maintainability:**
- NFR8: Code modulaire pour ajout de fonctionnalités
- NFR9: Composants Vulkan isolés pour apprentissage progressif
- NFR10: Compilation cross-platform sans modifications excessives

**Usability:**
- NFR11: Interface intuitive sans documentation requise
- NFR12: Messages d'erreur compréhensibles

### Additional Requirements

**From Architecture - Starter Template:**
- Minimal Custom Setup (pas de template externe, setup from scratch)
- C++20 standard avec CMake
- GLFW, VMA, Dear ImGUI via FetchContent

**From Architecture - Implementation Sequence:**
1. CMake setup + dépendances (GLFW, VMA, ImGUI)
2. Vulkan initialization (Instance, Device, Swapchain wrappers)
3. Rendering pipeline (Forward renderer)
4. Mesh loading (OBJ/STL parsers → indexed mesh)
5. Camera system (Trackball navigation)
6. UI integration (ImGUI with abstraction layer)
7. Info panel (Bounding box, stats display)

**From Architecture - Technical Constraints:**
- RAII mandatory for all Vulkan resources
- Return codes for error handling (no exceptions)
- Interleaved vertex buffers, indexed mesh
- Forward rendering pipeline
- Module boundaries: Core, Math, Loaders, Scene, Renderer, UI

### FR Coverage Map

| FR | Epic | Description |
|----|------|-------------|
| FR1 | Epic 3 | Ouvrir fichier via menu |
| FR2 | Epic 3 | Ouvrir fichier drag & drop |
| FR3 | Epic 3 | Charger OBJ |
| FR4 | Epic 3 | Charger STL |
| FR5 | Epic 3 | Détecter fichiers invalides |
| FR6 | Epic 3 | Retour état stable après erreur |
| FR7 | Epic 2 | Afficher modèle via Vulkan |
| FR8 | Epic 3 | Centrer auto après chargement |
| FR9 | Epic 5 | Afficher bounding box visuelle |
| FR10 | Epic 4 | Rotation trackball |
| FR11 | Epic 4 | Zoom molette |
| FR12 | Epic 4 | Pan |
| FR13 | Epic 5 | Dimensions bounding box |
| FR14 | Epic 5 | Position modèle |
| FR15 | Epic 5 | Nombre de faces |
| FR16 | Epic 5 | Nombre de sommets |
| FR17 | Epic 5 | Panneau info dédié |
| FR18 | Epic 5 | Messages erreur clairs |
| FR19 | Epic 5 | Quitter proprement |
| FR20 | Epic 1 | Windows support |
| FR21 | Epic 1 | Linux support |
| FR22 | Epic 1 | macOS support |

## Epic List

### Epic 1: Fondation Application & Fenêtre Vulkan
L'utilisateur peut lancer l'application sur sa plateforme (Windows/Linux/macOS) et voir une fenêtre Vulkan fonctionnelle.
**FRs couverts:** FR20, FR21, FR22

### Epic 2: Rendu 3D de Base
L'utilisateur peut voir un modèle 3D rendu via Vulkan (modèle de test intégré pour valider le pipeline).
**FRs couverts:** FR7

### Epic 3: Chargement de Modèles
L'utilisateur peut charger ses propres fichiers 3D (OBJ/STL) et les voir affichés, centrés automatiquement.
**FRs couverts:** FR1, FR2, FR3, FR4, FR5, FR6, FR8

### Epic 4: Navigation Caméra
L'utilisateur peut naviguer librement autour du modèle avec la souris.
**FRs couverts:** FR10, FR11, FR12

### Epic 5: Informations Modèle & Interface
L'utilisateur peut consulter toutes les informations du modèle dans un panneau dédié et quitter proprement.
**FRs couverts:** FR9, FR13, FR14, FR15, FR16, FR17, FR18, FR19

---

## Epic 1: Fondation Application & Fenêtre Vulkan

L'utilisateur peut lancer l'application sur sa plateforme (Windows/Linux/macOS) et voir une fenêtre Vulkan fonctionnelle.

### Story 1.1: Setup Projet CMake avec Dépendances

As a développeur,
I want configurer le projet CMake avec toutes les dépendances,
So that je puisse compiler l'application sur Windows, Linux et macOS.

**Acceptance Criteria:**

**Given** le code source est cloné sur une machine avec CMake 3.20+ et un compilateur C++20
**When** j'exécute `cmake -B build && cmake --build build`
**Then** le projet compile sans erreur
**And** GLFW, VMA et Dear ImGUI sont récupérés via FetchContent
**And** la structure de dossiers suit l'architecture définie (include/Vecna/, src/, shaders/, etc.)

**Given** je compile sur Windows avec MSVC 19.29+
**When** le build termine
**Then** un exécutable vecna.exe est généré

**Given** je compile sur Linux avec GCC 10+ ou Clang 10+
**When** le build termine
**Then** un exécutable vecna est généré

**Given** je compile sur macOS avec Clang
**When** le build termine
**Then** un exécutable vecna est généré avec support MoltenVK

### Story 1.2: Création Fenêtre GLFW

As a utilisateur,
I want lancer l'application et voir une fenêtre,
So that j'ai une interface visuelle pour interagir avec l'application.

**Acceptance Criteria:**

**Given** l'application est lancée
**When** l'initialisation est terminée
**Then** une fenêtre de 1280x720 pixels s'affiche avec le titre "Vecna"
**And** la fenêtre est redimensionnable

**Given** la fenêtre est affichée
**When** je clique sur le bouton de fermeture (X)
**Then** l'application se ferme proprement sans crash ni fuite mémoire

**Given** la fenêtre est affichée
**When** je redimensionne la fenêtre
**Then** la fenêtre accepte le redimensionnement

### Story 1.3: Instance Vulkan et Validation

As a développeur,
I want initialiser Vulkan avec les validation layers,
So that je puisse détecter les erreurs Vulkan pendant le développement.

**Acceptance Criteria:**

**Given** l'application démarre
**When** l'instance Vulkan est créée
**Then** VkInstance est créé avec succès
**And** les extensions requises pour GLFW sont activées
**And** le logging affiche "[Renderer] Vulkan instance created"

**Given** l'application tourne en mode Debug
**When** une erreur Vulkan se produit
**Then** le debug messenger capture l'erreur
**And** le message est affiché dans les logs avec le niveau approprié

**Given** l'application tourne en mode Release
**When** l'instance est créée
**Then** les validation layers ne sont pas activées (performance)

### Story 1.4: Sélection Device Vulkan

As a développeur,
I want sélectionner le GPU approprié et créer le device logique,
So that je puisse utiliser les capacités graphiques du système.

**Acceptance Criteria:**

**Given** l'instance Vulkan est créée
**When** les physical devices sont énumérés
**Then** un GPU avec support graphique Vulkan est sélectionné
**And** les GPUs discrets sont préférés aux GPUs intégrés
**And** le logging affiche le nom du GPU sélectionné

**Given** un physical device est sélectionné
**When** le logical device est créé
**Then** une queue graphique est disponible
**And** VMA (Vulkan Memory Allocator) est initialisé avec succès
**And** le logging affiche "[Renderer] Logical device created"

**Given** aucun GPU Vulkan n'est disponible
**When** l'énumération des devices échoue
**Then** un message d'erreur clair est affiché
**And** l'application se ferme proprement avec un code d'erreur

### Story 1.5: Swapchain et Présentation

As a utilisateur,
I want voir la fenêtre avec un rendu Vulkan actif,
So that je sache que le pipeline graphique fonctionne.

**Acceptance Criteria:**

**Given** le device Vulkan est initialisé
**When** la swapchain est créée
**Then** les images de la swapchain sont disponibles
**And** le format de surface est approprié (SRGB préféré)
**And** le mode de présentation est sélectionné (Mailbox > Fifo)

**Given** la swapchain est créée
**When** la boucle de rendu tourne
**Then** la fenêtre affiche une couleur de fond (clear color)
**And** le rendu est présenté sans erreur
**And** le framerate est stable

**Given** la fenêtre est redimensionnée
**When** les dimensions changent
**Then** la swapchain est recréée avec les nouvelles dimensions
**And** le rendu continue sans interruption visible

**Given** l'application se ferme
**When** le cleanup est exécuté
**Then** toutes les ressources Vulkan sont libérées via RAII
**And** aucune validation error n'est reportée

---

## Epic 2: Rendu 3D de Base

L'utilisateur peut voir un modèle 3D rendu via Vulkan (modèle de test intégré pour valider le pipeline).

### Story 2.1: Pipeline Graphique

As a développeur,
I want créer le pipeline graphique Vulkan,
So that je puisse rendre des primitives 3D à l'écran.

**Acceptance Criteria:**

**Given** les fichiers shaders basic.vert et basic.frag existent
**When** le build CMake est exécuté
**Then** glslc compile les shaders en SPIR-V dans shaders/compiled/
**And** les fichiers .spv sont générés sans erreur

**Given** les shaders SPIR-V sont disponibles
**When** le pipeline est créé
**Then** les shader modules sont chargés correctement
**And** le pipeline layout est créé avec les push constants pour MVP
**And** le graphics pipeline est créé avec le vertex input correspondant au format interleaved

**Given** le pipeline est créé
**When** le rendu est exécuté
**Then** le pipeline peut être bindé dans le command buffer
**And** le logging affiche "[Renderer] Graphics pipeline created"

### Story 2.2: Vertex et Index Buffers

As a développeur,
I want créer des buffers GPU pour les vertices et indices,
So that je puisse envoyer des données de mesh au GPU.

**Acceptance Criteria:**

**Given** VMA est initialisé
**When** un vertex buffer est créé avec des données
**Then** le buffer est alloué en GPU memory via VMA
**And** les données sont transférées via staging buffer
**And** le format est interleaved (position vec3, normal vec3, color vec3)

**Given** VMA est initialisé
**When** un index buffer est créé avec des indices
**Then** le buffer est alloué en GPU memory
**And** les indices uint32 sont transférés correctement

**Given** les buffers sont créés
**When** l'application se ferme
**Then** les buffers sont libérés automatiquement via RAII
**And** VMA ne reporte aucune fuite mémoire

### Story 2.3: Triangle de Test

As a utilisateur,
I want voir un triangle coloré à l'écran,
So that je sache que le pipeline de rendu fonctionne.

**Acceptance Criteria:**

**Given** le pipeline graphique est créé
**When** la boucle de rendu tourne
**Then** un triangle coloré est affiché au centre de l'écran
**And** chaque vertex a une couleur différente (interpolation visible)

**Given** le triangle est rendu
**When** je redimensionne la fenêtre
**Then** le triangle reste visible et correctement proportionné

**Given** le triangle est rendu
**When** j'observe les performances
**Then** le framerate est supérieur à 60 FPS

### Story 2.4: Cube 3D avec Depth Buffer

As a utilisateur,
I want voir un cube 3D correctement rendu,
So that je valide que le rendu 3D avec profondeur fonctionne.

**Acceptance Criteria:**

**Given** le pipeline est fonctionnel avec le triangle
**When** le depth buffer est ajouté
**Then** un depth attachment est créé avec format approprié (D32_SFLOAT ou D24_UNORM_S8_UINT)
**And** le render pass inclut le depth attachment
**And** depth testing est activé dans le pipeline

**Given** le depth buffer est configuré
**When** un cube 3D est rendu
**Then** les faces avant occultent correctement les faces arrière
**And** le cube est visible avec ses 6 faces distinctes

**Given** les matrices MVP sont implémentées
**When** le cube est rendu
**Then** la matrice Model positionne le cube dans l'espace monde
**And** la matrice View simule une caméra fixe regardant le cube
**And** la matrice Projection applique une perspective correcte

**Given** un éclairage basique est appliqué
**When** le cube est rendu
**Then** les faces ont des intensités différentes selon leur orientation
**And** l'éclairage utilise une direction de lumière fixe (hardcodée)

---

## Epic 3: Chargement de Modèles

L'utilisateur peut charger ses propres fichiers 3D (OBJ/STL) et les voir affichés, centrés automatiquement.

### Story 3.1: Parser OBJ

As a utilisateur,
I want charger des fichiers OBJ,
So that je puisse visualiser mes modèles 3D au format OBJ.

**Acceptance Criteria:**

**Given** un fichier .obj valide avec vertices et faces
**When** le parser OBJ lit le fichier
**Then** les positions des vertices (v) sont extraites correctement
**And** les normales (vn) sont extraites si présentes
**And** les faces (f) sont triangulées et indexées

**Given** un fichier .obj sans normales
**When** le parser lit le fichier
**Then** les normales sont calculées automatiquement par face

**Given** un fichier .obj avec des faces quad
**When** le parser lit le fichier
**Then** les quads sont automatiquement triangulés en 2 triangles

**Given** le parsing réussit
**When** les données sont retournées
**Then** le format est compatible avec le vertex buffer interleaved
**And** le logging affiche "[Loader] Loading model: filename.obj"

### Story 3.2: Parser STL

As a utilisateur,
I want charger des fichiers STL,
So that je puisse visualiser mes modèles 3D au format STL.

**Acceptance Criteria:**

**Given** un fichier .stl binaire valide
**When** le parser STL lit le fichier
**Then** le header est ignoré (80 bytes)
**And** le nombre de triangles est lu correctement
**And** chaque triangle (normale + 3 vertices) est extrait

**Given** un fichier .stl ASCII valide
**When** le parser détecte le format ASCII (commence par "solid")
**Then** le fichier est parsé en mode texte
**And** les triangles sont extraits correctement

**Given** le parsing STL réussit
**When** les données sont converties en mesh indexé
**Then** les vertices dupliqués sont fusionnés (welding)
**And** les indices sont générés pour optimiser la mémoire

### Story 3.3: Structure Mesh et Model

As a développeur,
I want des structures de données pour les meshes et modèles,
So that je puisse manipuler les données 3D de manière cohérente.

**Acceptance Criteria:**

**Given** un mesh est créé à partir de données parsées
**When** la structure Mesh est initialisée
**Then** les vertices interleaved sont stockés (position, normal, color)
**And** les indices sont stockés
**And** la bounding box est calculée automatiquement

**Given** un mesh est créé
**When** la BoundingBox est calculée
**Then** les min/max en X, Y, Z sont corrects
**And** le centre et les dimensions sont disponibles

**Given** un Model est créé avec un Mesh
**When** le Model est initialisé
**Then** les buffers GPU sont créés via VMA
**And** le modèle est prêt pour le rendu
**And** les statistiques (vertex count, face count) sont accessibles

### Story 3.4: Chargement via Menu Fichier

As a utilisateur,
I want ouvrir un fichier via le menu,
So that je puisse charger un modèle de manière traditionnelle.

**Acceptance Criteria:**

**Given** l'application est lancée
**When** je clique sur Menu > Fichier > Ouvrir (ou Ctrl+O)
**Then** un dialogue de sélection de fichier s'ouvre
**And** seuls les fichiers .obj et .stl sont affichés par défaut

**Given** le dialogue est ouvert
**When** je sélectionne un fichier valide et confirme
**Then** le modèle est chargé et affiché
**And** le modèle précédent est remplacé

**Given** le dialogue est ouvert
**When** j'annule la sélection
**Then** l'état actuel est préservé
**And** aucun changement n'est effectué

### Story 3.5: Chargement via Drag & Drop

As a utilisateur,
I want glisser-déposer un fichier sur la fenêtre,
So that je puisse charger un modèle rapidement.

**Acceptance Criteria:**

**Given** l'application est lancée
**When** je glisse un fichier .obj ou .stl sur la fenêtre
**Then** le curseur indique que le drop est accepté

**Given** un fichier est glissé sur la fenêtre
**When** je relâche le fichier
**Then** le modèle est chargé et affiché
**And** le modèle précédent est remplacé

**Given** un fichier non supporté est glissé
**When** je relâche le fichier
**Then** un message d'erreur indique le format non supporté
**And** l'état actuel est préservé

### Story 3.6: Centrage Automatique

As a utilisateur,
I want que le modèle soit automatiquement centré et visible,
So that je puisse le voir immédiatement sans manipulation.

**Acceptance Criteria:**

**Given** un modèle est chargé
**When** le centrage automatique est appliqué
**Then** le centre de la bounding box est à l'origine (0, 0, 0)
**And** le modèle est entièrement visible dans la vue

**Given** un modèle très petit ou très grand est chargé
**When** la mise à l'échelle adaptative est appliquée
**Then** le modèle occupe une proportion raisonnable de la vue
**And** la caméra est positionnée à une distance appropriée

**Given** le centrage est appliqué
**When** j'observe le modèle
**Then** l'orientation initiale montre une vue pertinente (face avant ou isométrique)

### Story 3.7: Gestion des Erreurs de Chargement

As a utilisateur,
I want être informé clairement des erreurs de chargement,
So that je comprenne pourquoi un fichier ne peut pas être ouvert.

**Acceptance Criteria:**

**Given** un fichier inexistant est sélectionné
**When** le chargement est tenté
**Then** l'erreur Loader::Error::FileNotFound est retournée
**And** un message "Fichier non trouvé: [chemin]" est affiché

**Given** un fichier avec extension invalide est sélectionné
**When** le chargement est tenté
**Then** l'erreur Loader::Error::InvalidFormat est retournée
**And** un message "Format non supporté: [extension]" est affiché

**Given** un fichier corrompu ou mal formé est chargé
**When** le parsing échoue
**Then** l'erreur Loader::Error::ParseError est retournée
**And** un message descriptif de l'erreur est affiché
**And** le logging affiche "[Loader] ERROR: [détails]"

**Given** une erreur de chargement survient
**When** l'utilisateur voit le message d'erreur
**Then** le modèle précédent reste affiché (si existant)
**And** l'application reste dans un état stable
**And** l'utilisateur peut retenter avec un autre fichier

---

## Epic 4: Navigation Caméra

L'utilisateur peut naviguer librement autour du modèle avec la souris.

### Story 4.1: Classe Camera

As a développeur,
I want une classe Camera gérant les matrices de vue,
So that je puisse contrôler le point de vue sur le modèle.

**Acceptance Criteria:**

**Given** une Camera est créée
**When** les paramètres sont initialisés
**Then** la position, le target et le vecteur up sont définis
**And** le field of view est configurable (défaut: 45°)
**And** les plans near/far sont configurés

**Given** la Camera existe
**When** getViewMatrix() est appelée
**Then** une matrice de vue correcte est retournée
**And** la matrice transforme les coordonnées monde vers vue

**Given** la Camera existe
**When** getProjectionMatrix() est appelée avec l'aspect ratio
**Then** une matrice de projection perspective est retournée
**And** l'aspect ratio est pris en compte

**Given** la fenêtre est redimensionnée
**When** l'aspect ratio change
**Then** la matrice de projection est mise à jour
**And** le rendu n'est pas déformé

### Story 4.2: Rotation Trackball

As a utilisateur,
I want faire tourner le modèle avec la souris,
So that je puisse l'examiner sous tous les angles.

**Acceptance Criteria:**

**Given** un modèle est affiché
**When** je clique gauche et je drag horizontalement
**Then** le modèle tourne autour de l'axe vertical (Y)
**And** la rotation suit le mouvement de la souris

**Given** un modèle est affiché
**When** je clique gauche et je drag verticalement
**Then** le modèle tourne autour de l'axe horizontal
**And** la rotation est limitée pour éviter le gimbal lock

**Given** un modèle est affiché
**When** je fais une rotation en diagonale
**Then** les deux axes de rotation sont combinés
**And** le mouvement est fluide et intuitif (type trackball)

**Given** je relâche le bouton de souris
**When** la rotation s'arrête
**Then** la position de la caméra est stable
**And** aucun drift ou mouvement résiduel

### Story 4.3: Zoom Molette

As a utilisateur,
I want zoomer avec la molette de la souris,
So that je puisse voir les détails ou la vue d'ensemble.

**Acceptance Criteria:**

**Given** un modèle est affiché
**When** je scroll vers le haut (molette avant)
**Then** la caméra se rapproche du modèle (zoom in)
**And** le mouvement est progressif et fluide

**Given** un modèle est affiché
**When** je scroll vers le bas (molette arrière)
**Then** la caméra s'éloigne du modèle (zoom out)
**And** le mouvement est progressif et fluide

**Given** je zoome très près
**When** la distance minimale est atteinte
**Then** le zoom s'arrête à la limite
**And** la caméra ne traverse pas le modèle

**Given** je zoome très loin
**When** la distance maximale est atteinte
**Then** le zoom s'arrête à la limite
**And** le modèle reste visible à l'écran

### Story 4.4: Pan (Déplacement Latéral)

As a utilisateur,
I want déplacer la vue latéralement,
So that je puisse centrer une zone d'intérêt spécifique.

**Acceptance Criteria:**

**Given** un modèle est affiché
**When** je clique avec le bouton milieu et je drag
**Then** la vue se déplace dans le plan de l'écran
**And** le déplacement suit le mouvement de la souris

**Given** un modèle est affiché
**When** je maintiens Shift + clic gauche et je drag
**Then** la vue se déplace (alternative au bouton milieu)
**And** le comportement est identique au pan avec bouton milieu

**Given** je fais un pan
**When** le déplacement est appliqué
**Then** le point cible de la caméra est mis à jour
**And** les rotations futures tournent autour du nouveau centre

**Given** je combine pan et rotation
**When** les deux opérations sont enchaînées
**Then** chaque opération fonctionne correctement
**And** la navigation reste intuitive et prévisible

---

## Epic 5: Informations Modèle & Interface

L'utilisateur peut consulter toutes les informations du modèle dans un panneau dédié et quitter proprement.

### Story 5.1: Abstraction Interface UI

As a développeur,
I want une interface abstraite pour le système UI,
So that je puisse changer de framework UI sans modifier le code métier.

**Acceptance Criteria:**

**Given** l'interface IUIRenderer est définie
**When** une implémentation est créée
**Then** les méthodes beginFrame(), endFrame(), render() sont disponibles
**And** l'interface ne dépend pas d'ImGUI directement

**Given** l'abstraction UI existe
**When** le code métier utilise l'UI
**Then** il interagit uniquement via IUIRenderer
**And** aucune dépendance directe à ImGUI dans les autres modules

**Given** l'architecture est modulaire
**When** on souhaite changer de framework UI
**Then** seule l'implémentation de IUIRenderer doit être modifiée
**And** le reste du code reste inchangé

### Story 5.2: Implémentation ImGUI

As a développeur,
I want intégrer Dear ImGUI avec Vulkan,
So that je puisse afficher des éléments d'interface utilisateur.

**Acceptance Criteria:**

**Given** ImGUI est disponible via FetchContent
**When** ImGuiRenderer est initialisé
**Then** le contexte ImGUI est créé
**And** les backends GLFW et Vulkan sont configurés
**And** le logging affiche "[UI] ImGUI initialized"

**Given** ImGuiRenderer est initialisé
**When** beginFrame() est appelé
**Then** un nouveau frame ImGUI commence
**And** les inputs sont capturés depuis GLFW

**Given** des éléments UI sont créés
**When** endFrame() puis render() sont appelés
**Then** les draw commands ImGUI sont générées
**And** le rendu est intégré au command buffer Vulkan

**Given** l'application se ferme
**When** ImGuiRenderer est détruit
**Then** toutes les ressources ImGUI sont libérées
**And** aucune fuite mémoire n'est reportée

### Story 5.3: Panneau d'Information Modèle

As a utilisateur,
I want voir les informations du modèle dans un panneau,
So that je puisse connaître ses caractéristiques géométriques.

**Acceptance Criteria:**

**Given** un modèle est chargé
**When** le panneau InfoPanel est affiché
**Then** le nom du fichier est affiché
**And** le panneau est positionné dans un coin de l'écran

**Given** le panneau est affiché
**When** je consulte les dimensions
**Then** les dimensions de la bounding box sont affichées (X, Y, Z)
**And** les valeurs sont en unités du modèle avec précision appropriée

**Given** le panneau est affiché
**When** je consulte la position
**Then** le centre de la bounding box est affiché
**And** les coordonnées min et max sont disponibles

**Given** le panneau est affiché
**When** je consulte les statistiques
**Then** le nombre de faces (triangles) est affiché
**And** le nombre de sommets est affiché

**Given** les informations sont affichées
**When** le modèle change
**Then** les informations sont mises à jour en < 100ms
**And** l'affichage reste stable sans clignotement

### Story 5.4: Affichage Bounding Box Visuelle

As a utilisateur,
I want voir la bounding box du modèle en 3D,
So that je puisse visualiser son encombrement spatial.

**Acceptance Criteria:**

**Given** un modèle est chargé
**When** l'affichage bounding box est activé
**Then** un wireframe cubique est rendu autour du modèle
**And** les arêtes correspondent aux limites exactes du modèle

**Given** la bounding box est affichée
**When** je navigue autour du modèle
**Then** la bounding box reste alignée avec le modèle
**And** elle tourne avec le modèle

**Given** le panneau InfoPanel est visible
**When** je clique sur un toggle "Afficher BBox"
**Then** l'affichage de la bounding box est activé/désactivé
**And** l'état est mémorisé pendant la session

**Given** la bounding box est rendue
**When** j'observe le rendu
**Then** la couleur est distincte du modèle (ex: jaune ou cyan)
**And** les lignes ne masquent pas le modèle (rendu after ou transparence)

### Story 5.5: Messages d'Erreur UI

As a utilisateur,
I want voir les messages d'erreur clairement,
So that je comprenne ce qui s'est passé.

**Acceptance Criteria:**

**Given** une erreur survient (chargement, Vulkan, etc.)
**When** le message d'erreur est affiché
**Then** une notification/popup apparaît à l'écran
**And** le message est clair et compréhensible

**Given** un message d'erreur est affiché
**When** quelques secondes passent (5s)
**Then** le message disparaît automatiquement
**And** l'interface revient à la normale

**Given** un message d'erreur est affiché
**When** je clique sur le message ou un bouton "X"
**Then** le message disparaît immédiatement
**And** je peux continuer à utiliser l'application

**Given** plusieurs erreurs surviennent
**When** les messages sont affichés
**Then** ils sont empilés ou en file d'attente
**And** chaque message est visible et lisible

### Story 5.6: Fermeture Propre Application

As a utilisateur,
I want quitter l'application proprement,
So that toutes les ressources soient libérées correctement.

**Acceptance Criteria:**

**Given** l'application est en cours d'exécution
**When** je sélectionne Menu > Fichier > Quitter
**Then** l'application se ferme
**And** toutes les ressources sont libérées

**Given** l'application est en cours d'exécution
**When** j'utilise Alt+F4 (Windows/Linux) ou Cmd+Q (macOS)
**Then** l'application se ferme proprement

**Given** l'application se ferme
**When** le cleanup est exécuté
**Then** les ressources Vulkan sont libérées dans l'ordre correct
**And** VMA ne reporte aucune fuite mémoire
**And** les validation layers ne reportent aucune erreur

**Given** l'application se ferme
**When** j'observe le code de sortie
**Then** le code est 0 (succès) en cas de fermeture normale
**And** un code non-zéro indique une erreur

---
stepsCompleted: ['step-01-init', 'step-02-discovery', 'step-03-success', 'step-04-journeys', 'step-05-domain', 'step-06-innovation', 'step-07-project-type', 'step-08-scoping', 'step-09-functional', 'step-10-nonfunctional', 'step-11-polish']
inputDocuments: []
workflowType: 'prd'
documentCounts:
  briefs: 0
  research: 0
  brainstorming: 0
  projectDocs: 0
classification:
  projectType: desktop_app
  domain: scientific
  complexity: medium
  projectContext: greenfield
  targetPlatforms: [Windows, Linux, Mac]
  initialFormats: [OBJ, STL]
  purpose: learning_vulkan
---

# Product Requirements Document - Vecna

**Author:** C_lau
**Date:** 2026-01-26

## Executive Summary

**Vecna** est un viewer 3D multi-plateforme basé sur l'API Vulkan, conçu pour visualiser des modèles 3D et obtenir rapidement des informations sur leurs caractéristiques géométriques.

**Vision** : Fournir un outil simple et performant pour inspecter des modèles 3D (OBJ, STL) tout en servant de projet d'apprentissage pour maîtriser Vulkan.

**Différenciateur** : Projet personnel centré sur l'apprentissage de Vulkan avec une approche incrémentale et modulaire.

**Utilisateur cible** : Développeur/chercheur souhaitant visualiser et analyser rapidement des modèles 3D.

**Plateformes** : Windows, Linux, macOS

## Success Criteria

### User Success

**MVP (Phase 1) :**
- Charger et visualiser des modèles OBJ et STL sans friction
- Naviguer fluidement autour du modèle (rotation, zoom, pan)
- Obtenir instantanément les informations globales (bounding box, dimensions, statistiques)

**Phase 2 :**
- Sélectionner et inspecter n'importe quel élément (face, sommet)

### Technical Success

| Métrique | Cible |
|----------|-------|
| FPS minimum | 60 FPS |
| Taille modèle supportée | 1M+ triangles |
| Formats supportés | OBJ, STL |
| Plateformes | Windows, Linux, macOS |

### Learning Success

- Maîtrise du pipeline de rendu Vulkan
- Compréhension de la gestion mémoire GPU
- Maîtrise de la synchronisation Vulkan
- Code maintenable et évolutif

## Product Scope

### Phase 1 - MVP

| Fonctionnalité | Priorité | Justification |
|----------------|----------|---------------|
| Rendu Vulkan fonctionnel | P0 | Objectif d'apprentissage principal |
| Chargement fichiers OBJ | P0 | Format standard répandu |
| Chargement fichiers STL | P0 | Format simple et courant |
| Navigation trackball | P0 | Interaction de base essentielle |
| Affichage bounding box | P0 | Information visuelle immédiate |
| Infos modèle global | P0 | Feedback utilisateur rapide |
| Drag & drop fichiers | P0 | UX fluide |
| Build multi-plateforme | P0 | Portabilité requise |

**User Journeys supportés :** Inspection rapide (partiel), Gestion des erreurs

### Phase 2 - Enrichissement

- Sélection de face et de sommet
- Affichage infos élément sélectionné
- Modes de rendu alternatifs (wireframe, normales)

**User Journeys supportés :** Exploration détaillée (complet)

### Phase 3 - Expansion

- Formats additionnels (GLTF, FBX)
- Outils de mesure (distances, angles)
- Export captures d'écran
- Support multi-objets, PBR, animations, plugins

### Constraints

- **Vulkan obligatoire** : Pas de fallback OpenGL (objectif d'apprentissage)
- **Ressources** : Projet personnel, temps libre

## User Journeys

### Parcours 1 : Inspection Rapide

**Persona** : C_lau - Développeur/Chercheur

**Situation** : Réception d'un fichier 3D d'une source quelconque.

**Parcours :**
1. Lancement de Vecna, ouverture fichier via menu ou drag & drop
2. Modèle affiché instantanément, centré dans la vue
3. Consultation des informations : bounding box, position, nombre faces/sommets
4. Décision rapide sur l'exploitabilité du modèle

**Résultat** : Informations essentielles disponibles en quelques secondes.

### Parcours 2 : Exploration Détaillée (Phase 2)

**Situation** : Besoin de comprendre la structure géométrique d'un modèle.

**Parcours :**
1. Navigation trackball autour du modèle
2. Zoom sur zone d'intérêt
3. Sélection d'une face ou sommet
4. Inspection des propriétés (normale, aire, position, adjacences)

**Résultat** : Compréhension complète de la topologie du maillage.

### Parcours 3 : Gestion des Erreurs

**Situation** : Fichier corrompu ou incompatible.

**Parcours :**
1. Tentative d'ouverture d'un fichier problématique
2. Détection de l'erreur par le parser
3. Message d'erreur clair
4. Retour à l'état précédent

**Résultat** : Pas de crash, feedback clair.

## Technical Requirements

### Platform Support

| Plateforme | Support | Notes |
|------------|---------|-------|
| Windows | Oui | Vulkan SDK, builds MSVC ou MinGW |
| Linux | Oui | Vulkan SDK, builds GCC/Clang |
| macOS | Oui | MoltenVK (Vulkan sur Metal) |

### Architecture

- **Rendu** : API Vulkan
- **Fenêtrage** : GLFW, SDL2, ou similaire (cross-platform)
- **Build** : CMake
- **Dépendances** : Vulkan SDK, bibliothèque de fenêtrage, parsers OBJ/STL

### System Integration

| Fonctionnalité | Statut |
|----------------|--------|
| Drag & drop fichiers | Inclus |
| Association fichiers | Exclu |
| Menu contextuel système | Exclu |
| Mise à jour automatique | Exclu |

### Risk Mitigation

| Risque | Mitigation |
|--------|------------|
| Complexité Vulkan | Suivre vulkan-tutorial.com, progression incrémentale |
| Cross-platform | Utiliser GLFW ou SDL2 dès le départ |
| MoltenVK sur macOS | Tester régulièrement sur Mac |

## Functional Requirements

### File Management

- **FR1**: L'utilisateur peut ouvrir un fichier 3D via le menu Fichier
- **FR2**: L'utilisateur peut ouvrir un fichier 3D par drag & drop
- **FR3**: L'utilisateur peut charger des fichiers au format OBJ
- **FR4**: L'utilisateur peut charger des fichiers au format STL
- **FR5**: Le système détecte et signale les fichiers corrompus ou invalides
- **FR6**: Le système retourne à un état stable après une erreur de chargement

### 3D Rendering

- **FR7**: Le système affiche le modèle 3D via l'API Vulkan
- **FR8**: Le système centre automatiquement le modèle après chargement
- **FR9**: Le système affiche visuellement la bounding box du modèle

### Camera Navigation

- **FR10**: L'utilisateur peut effectuer une rotation trackball (souris)
- **FR11**: L'utilisateur peut zoomer (molette souris)
- **FR12**: L'utilisateur peut effectuer un pan (déplacement latéral)

### Model Information

- **FR13**: L'utilisateur peut consulter les dimensions de la bounding box (X, Y, Z)
- **FR14**: L'utilisateur peut consulter la position du modèle (origine, centre)
- **FR15**: L'utilisateur peut consulter le nombre de faces
- **FR16**: L'utilisateur peut consulter le nombre de sommets

### User Interface

- **FR17**: Le système affiche les informations du modèle dans un panneau dédié
- **FR18**: Le système affiche des messages d'erreur clairs
- **FR19**: L'utilisateur peut quitter l'application proprement

### Cross-Platform

- **FR20**: L'application fonctionne sur Windows
- **FR21**: L'application fonctionne sur Linux
- **FR22**: L'application fonctionne sur macOS

## Non-Functional Requirements

### Performance

- **NFR1**: 60+ FPS avec modèles jusqu'à 1M triangles
- **NFR2**: Chargement fichier 1M triangles < 5 secondes
- **NFR3**: Navigation fluide sans latence perceptible
- **NFR4**: Mise à jour informations < 100ms

### Reliability

- **NFR5**: Pas de crash sur fichiers invalides
- **NFR6**: Récupération gracieuse des erreurs GPU/Vulkan
- **NFR7**: État cohérent après erreur

### Maintainability

- **NFR8**: Code modulaire pour ajout de fonctionnalités
- **NFR9**: Composants Vulkan isolés pour apprentissage progressif
- **NFR10**: Compilation cross-platform sans modifications excessives

### Usability

- **NFR11**: Interface intuitive sans documentation requise
- **NFR12**: Messages d'erreur compréhensibles

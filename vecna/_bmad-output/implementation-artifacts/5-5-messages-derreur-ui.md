# Story 5.5: Messages d'Erreur UI

Status: done

## Story

As a utilisateur,
I want voir les messages d'erreur clairement,
so that je comprenne ce qui s'est passé.

## Acceptance Criteria

1. **AC1 - Notification d'erreur à l'écran**
   - Given une erreur survient (chargement, Vulkan, etc.)
   - When le message d'erreur est affiché
   - Then une notification/popup apparaît à l'écran
   - And le message est clair et compréhensible

2. **AC2 - Disparition automatique après 5 secondes**
   - Given un message d'erreur est affiché
   - When quelques secondes passent (5s)
   - Then le message disparaît automatiquement
   - And l'interface revient à la normale

3. **AC3 - Fermeture manuelle par clic**
   - Given un message d'erreur est affiché
   - When je clique sur le message ou un bouton "X"
   - Then le message disparaît immédiatement
   - And je peux continuer à utiliser l'application

4. **AC4 - Empilement de plusieurs erreurs**
   - Given plusieurs erreurs surviennent
   - When les messages sont affichés
   - Then ils sont empilés ou en file d'attente
   - And chaque message est visible et lisible

## Tasks / Subtasks

- [x] Task 1: Créer le struct ErrorMessage et la file d'attente (AC: #1, #2, #4)
  - [x] 1.1 Ajouter struct privé `ErrorMessage` dans ImGuiRenderer.hpp avec `std::string text` et `std::chrono::steady_clock::time_point timestamp`
  - [x] 1.2 Ajouter membre `std::vector<ErrorMessage> m_errorMessages` dans ImGuiRenderer
  - [x] 1.3 Ajouter constante `static constexpr float ERROR_DISPLAY_DURATION = 5.0f` (secondes)

- [x] Task 2: Ajouter la méthode showErrorMessage() (AC: #1, #4)
  - [x] 2.1 Ajouter méthode publique `void showErrorMessage(const std::string& message)` sur ImGuiRenderer (PAS sur IUIRenderer — pattern concret)
  - [x] 2.2 L'implémentation push_back un ErrorMessage avec timestamp = steady_clock::now()
  - [x] 2.3 Logger le message aussi via `Core::Logger::error("UI", message)` pour garder la trace console

- [x] Task 3: Implémenter renderErrorMessages() (AC: #1, #2, #3, #4)
  - [x] 3.1 Créer méthode privée `renderErrorMessages()` dans ImGuiRenderer
  - [x] 3.2 Appeler `renderErrorMessages()` dans `beginFrame()` après `renderInfoPanel()`
  - [x] 3.3 Implémenter le rendu ImGui :
    - Position: coin inférieur gauche (`ImGui::SetNextWindowPos` avec pivot `(0.0f, 1.0f)`)
    - Pas de titre de fenêtre (ImGuiWindowFlags_NoTitleBar)
    - Flags: AlwaysAutoResize, NoMove, NoSavedSettings, NoFocusOnAppearing, NoNav
    - Fond semi-transparent rouge/sombre pour les erreurs
  - [x] 3.4 Pour chaque message dans m_errorMessages :
    - Afficher le texte du message
    - Ajouter un bouton "X" (ImGui::SameLine + ImGui::SmallButton) pour fermeture manuelle (AC3)
    - Séparer les messages par ImGui::Separator() si plusieurs
  - [x] 3.5 Supprimer les messages expirés (> 5s depuis timestamp) au début de renderErrorMessages() (AC2)
  - [x] 3.6 Supprimer le message quand l'utilisateur clique "X" (AC3)
  - [x] 3.7 Ne pas afficher la fenêtre si m_errorMessages est vide

- [x] Task 4: Connecter les erreurs de loadModel() au UI (AC: #1, #5)
  - [x] 4.1 Dans Application::loadModel(), à chaque `Logger::error("Loader", ...)`, ajouter un appel `showErrorMessage()` via helper showUIError()
  - [x] 4.2 Messages utilisateur à afficher (PAS les messages techniques de log) :
    - Échec de chargement : `"Impossible de charger le fichier : " + path.filename().string()`
    - Bounding box vide : `"Le modele est vide ou invalide"`
    - Aucune face valide : `"Le modele ne contient aucune face valide"`
  - [x] 4.3 Pour le warning des faces ignorées : `"X faces ignorees (indices invalides)"`
  - [x] 4.4 Créer un helper privé `void showUIError(const std::string& message)` dans Application

- [x] Task 5: Écrire les tests (AC: #1, #2, #3, #4)
  - [x] 5.1 Test: ErrorMessage struct est privé — vérifié par compilation (struct dans private section)
  - [x] 5.2 Test: ImGuiRenderer reste final et hérite toujours de IUIRenderer après ajout de showErrorMessage
  - [x] 5.3 Test: ImGuiRenderer n'est toujours pas default-constructible
  - [x] 5.4 Test: Application.hpp compile toujours sans dépendance ImGui (test existant passe)

- [x] Task 6: Validation compilation et régression (AC: #1, #2, #3, #4)
  - [x] 6.1 Compilation Debug sans erreurs (vecna.exe + vecna_tests.exe)
  - [x] 6.2 Tous les tests passent sans régression (92/92)
  - [x] 6.3 Vérification manuelle : application démarre correctement, pas de crash

## Dev Notes

### Constat — Aucun feedback d'erreur visible pour l'utilisateur

Actuellement, toutes les erreurs de `loadModel()` sont loguées via `Logger::error()` (qui écrit sur `std::cerr`) mais l'utilisateur ne voit RIEN dans l'interface graphique. Le chargement échoue silencieusement. Il faut un mécanisme de notification visuelle.

### Architecture — Toasts/notifications dans ImGuiRenderer

**Décision** : Pas de modification à `IUIRenderer` — le mécanisme d'erreur est spécifique au concret `ImGuiRenderer`, comme `setOnFileOpen` et `setModelInfo`. L'application utilise `dynamic_cast` pour appeler la méthode sur le type concret.

**Pattern** : Identique à `setModelInfo()` (Story 5-3) — méthode publique sur ImGuiRenderer, méthode privée de rendu appelée dans `beginFrame()`.

### Position et style des notifications

- **Position** : Coin inférieur gauche pour éviter le conflit avec le panneau d'info modèle (coin supérieur droit)
- **Style** : Fond rouge/sombre semi-transparent, texte blanc, bouton "X" pour fermer
- **Empilement** : Les messages sont empilés verticalement dans une seule fenêtre ImGui
- **Auto-dismiss** : Chaque message a son propre timer de 5 secondes via `std::chrono::steady_clock`

### Timing — steady_clock pour l'auto-dismiss

Utiliser `std::chrono::steady_clock` (pas `system_clock`) car il est monotone et ne dépend pas de l'horloge système. Le calcul est simple :
```cpp
auto elapsed = std::chrono::steady_clock::now() - msg.timestamp;
if (elapsed > std::chrono::duration<float>(ERROR_DISPLAY_DURATION)) {
    // Supprimer le message
}
```

### Messages utilisateur vs messages techniques

Les messages dans l'UI doivent être en français et compréhensibles. Ne pas afficher les messages techniques de Logger::error() tels quels. Traduction :
- `"Failed to load model: path"` → `"Impossible de charger le fichier : filename"`
- `"Model has empty bounding box"` → `"Le modèle est vide ou invalide"`
- `"Model has no valid faces"` → `"Le modèle ne contient aucune face valide"`

### Helper showUIError dans Application

Pour éviter de dupliquer le `dynamic_cast` à chaque point d'erreur dans loadModel(), créer un helper privé :
```cpp
void Application::showUIError(const std::string& message) {
    if (auto* imgui = dynamic_cast<UI::ImGuiRenderer*>(m_uiRenderer.get())) {
        imgui->showErrorMessage(message);
    }
}
```

### Points d'erreur dans loadModel() — lignes actuelles

1. **Ligne ~427** : `mesh.load()` retourne non-zero → fichier invalide/introuvable
2. **Ligne ~439** : `mesh.bbox().GetCenter()` retourne false → bbox vide
3. **Ligne ~536** : `skippedFaces > 0` → warning (pas fatal)
4. **Ligne ~542** : `indices.empty()` → aucune face valide

### Logger — Pas de callback, messages explicites requis

Le `Logger` actuel est un simple writer vers stdout/stderr. Il n'a pas de mécanisme d'observer/callback. Les messages d'erreur UI doivent être poussés explicitement vers `ImGuiRenderer::showErrorMessage()`. On ne modifie PAS Logger pour cette story.

### Suppression des messages expirés

L'approche la plus simple : au début de `renderErrorMessages()`, itérer sur `m_errorMessages` et utiliser `std::erase_if` (C++20) pour supprimer les messages expirés. Puis itérer sur les restants pour le rendu.

```cpp
std::erase_if(m_errorMessages, [](const ErrorMessage& msg) {
    auto elapsed = std::chrono::steady_clock::now() - msg.timestamp;
    return elapsed > std::chrono::duration<float>(ERROR_DISPLAY_DURATION);
});
```

### Learnings des stories précédentes

- `Core::Logger::info()` qualifié complet depuis `Vecna::UI` namespace
- Méthodes spécifiques sur ImGuiRenderer (pas IUIRenderer) — pattern établi
- `dynamic_cast` dans Application pour appeler des méthodes concrètes (pattern 5-3)
- Tests compile-time avec type_traits
- Pas de nouveau fichier source — seulement modifications d'existants
- Include `<chrono>` nécessaire dans ImGuiRenderer.hpp pour steady_clock

### Conventions de code

- Namespace : `Vecna::UI` pour ImGuiRenderer, `Vecna::Core` pour Application
- PascalCase classes/structs, camelCase méthodes, `m_` préfixe membres
- `#pragma once` pour tous les headers
- Logging : `Core::Logger::error("UI", "...")`
- C++20 : `std::erase_if` disponible

### Project Structure Notes

Fichiers modifiés :
- `include/Vecna/UI/ImGuiRenderer.hpp` — ajout ErrorMessage struct, showErrorMessage(), renderErrorMessages(), m_errorMessages
- `src/UI/ImGuiRenderer.cpp` — implémentation showErrorMessage() et renderErrorMessages()
- `src/Core/Application.cpp` — showUIError() helper, appels dans loadModel()
- `include/Vecna/Core/Application.hpp` — déclaration showUIError()
- `tests/UI/UIAbstractionTest.cpp` — tests supplémentaires

Aucun nouveau fichier.

### References

- [Source: epics.md#Story 5.5] — ACs originaux
- [Source: architecture.md#Error Handling] — Dual strategy (exceptions vs return codes)
- [Source: architecture.md#UI Module] — UI depends on Core, Scene, Renderer
- [Source: Application.cpp:427-542] — Points d'erreur dans loadModel()
- [Source: ImGuiRenderer.cpp:118-130] — beginFrame() où ajouter renderErrorMessages()
- [Source: 5-3-panneau-dinformation-modele.md] — Pattern setModelInfo/dynamic_cast
- [Source: 5-1-abstraction-interface-ui.md] — Pattern setOnFileOpen sur concret
- [Source: Logger.hpp] — API Logger actuelle (pas de callback)

## Dev Agent Record

### Agent Model Used

Claude Opus 4.6

### Debug Log References

- Zero build errors on first compile attempt

### Completion Notes List

- Task 1: Created private `ErrorMessage` struct (text + steady_clock timestamp), `m_errorMessages` vector, `ERROR_DISPLAY_DURATION = 5.0f` constant in ImGuiRenderer.
- Task 2: `showErrorMessage()` pushes to vector with current timestamp, also logs via `Core::Logger::error("UI", ...)`.
- Task 3: `renderErrorMessages()` in beginFrame() — bottom-left position, dark red semi-transparent background, auto-dismiss after 5s via `std::erase_if`, "X" button per message for manual dismiss, stacking with separators.
- Task 4: `showUIError()` helper in Application (dynamic_cast pattern). Connected 4 error points in loadModel(): file load failure, empty bbox, skipped faces warning, no valid faces. User-facing messages in French.
- Task 5: 2 new compile-time tests (final+inheritance preserved, not default-constructible). Existing tests confirm no ImGui dependency leak.
- Task 6: Build OK, 92/92 tests pass, app starts correctly.

### Change Log

- 2026-02-27: Story 5-5 implementation — Error message toast notifications in ImGuiRenderer

### File List

- include/Vecna/UI/ImGuiRenderer.hpp (modified) — Added ErrorMessage struct, showErrorMessage(), renderErrorMessages(), m_errorMessages
- src/UI/ImGuiRenderer.cpp (modified) — Implemented showErrorMessage() and renderErrorMessages()
- include/Vecna/Core/Application.hpp (modified) — Added showUIError() declaration
- src/Core/Application.cpp (modified) — Implemented showUIError(), connected 4 error points in loadModel()
- tests/UI/UIAbstractionTest.cpp (modified) — Added 2 Story 5-5 compile-time tests

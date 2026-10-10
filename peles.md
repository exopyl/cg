# `peles` — analyse de faisabilité d'un équivalent de `sinaia` sous RmlUi

**Ouvert le** : 2026-09-22.
**Objet** : évaluer la faisabilité d'une application équivalente à `sinaia`, nommée `peles`, dont la
couche d'interface serait **RmlUi** (RML/RCSS) au lieu de **wxWidgets**.
**Statut** : analyse seule. Aucune ligne d'implémentation n'a été écrite. Toute suite passe par le
sous-agent `developer` puis `/code-review`.

**Motif retenu par l'utilisateur** (2026-09-22) : **style et thématisation** — sortir la présentation
du C++, obtenir des feuilles de style, le rechargement à chaud et l'itération visuelle.
**Piste retenue** : **piste 1, portage « big bang »** — `peles` est une cible neuve, `sinaia` reste
intact. Les cinq autres pistes explorées sont consignées au §5, avec leurs raisons d'écart.
**Contrainte de périmètre** : **onglets de documents oui, docking flottant non.** Le détachement en
fenêtre et `SavePerspective`/`LoadPerspective` sortent du périmètre.

---

## 0. Méthode, et ce qui n'a pas été examiné

**Discipline appliquée** : tout fait porte son `fichier:ligne` ou son URL. Ce qui relève d'une
inférence ou d'une hypothèse est étiqueté comme tel. Une absence constatée par recherche est un
**minorant** et le dit ; une absence constatée par lecture d'un index exhaustif est solide.

**Lu intégralement** : `sinaia/SinaiaFrame.{cpp,h}` (4 326 + 542 l.), `sinaia/wxOpenGLCanvas.{cpp,h}`
(2 317 + 583 l.), `CuttingMat.{h,cpp}`, `SinaiaApp.cpp`, `main.cpp`, `Managers.{h,cpp}`,
`ImportSettings.h`, `PanelSettings.h`, `{Curvature,Decimation,Property}Panel.h`, `SupportedFormats.h`,
`RemoteConsole.h`, `SettingsPanel.cpp`, `DrawingArea.cpp`, `wxSizeReportCtrl.cpp`,
`sinaia/CMakeLists.txt`, `CMakeLists.txt` racine, `CMakePresets.json`, les en-têtes de `src/cgre`,
`src/cgre2/CMakeLists.txt`, `src/cgnet/LineConsole.h`, `cggraph-boilerplate/main.cpp`,
`sulina/src/{main.cpp,CgreQuickItem.h,MeshModel.h}`, `maker/CMakeLists.txt`, `maker/web/js/wasm.js`,
`vecna/src/Core/FileDialog.cpp`, `debt_sinaia.md` (228 l.), et les sommaires et sections de décision de
`camera_refacto.md`, `debt_cgre.md`, `cgmesh_refacto.md`.

**NON examiné, et déclaré comme tel** :

| Élément | Volume | Conséquence sur cette analyse |
|---|---|---|
| `sinaia/RemoteConsole.cpp`, corps des commandes | ~1 230 l. sur 1 774 | Les affirmations portent sur la **structure** et les dépendances (grep exhaustif sur `wx`), pas sur la sémantique de chaque commande |
| `src/cgre/*.cpp` | 5 828 l. | Les en-têtes ont été lus ; le corps s'appuie sur `debt_cgre.md`, **source secondaire** |
| `sinaia/{Curvature,Decimation,Property}Panel.cpp` | 397 l. | Seuls leurs en-têtes ont été lus |
| `sinaia/icons.h` | 577 l. | Données XPM, sans portée d'architecture |
| `sulina/src/CgreQuickItem.cpp`, QML | 1 324 + 2 243 l. | **La piste 6 n'a pas pu être chiffrée** faute de cette lecture. Déclarée non estimée plutôt que devinée |
| `vecna/src/Core/Application.cpp` | 1 357 l. | Compté en volume, pas analysé |
| Le code source de **RmlUi** | — | **Absent d'`extern/`.** Tout le §2 repose sur la documentation, le dépôt GitHub et un projet tiers, jamais sur une lecture locale ni sur une exécution |

**Aucune exécution n'a eu lieu.** Aucun binaire n'a été construit, aucune mesure de performance n'a
été prise, aucune sonde n'a été lancée. Les verrous du §4 sont des **risques identifiés par lecture**,
pas des pannes constatées.

---

## 1. Ce qu'est `sinaia`, mesuré

### 1.1 Les 22 fonctions et leur dépendance à wx

| # | Fonction | Site principal | Service wx | Portabilité vers une UI sans widgets natifs |
|---|---|---|---|---|
| 1 | Menus + accélérateurs (5 menus, ~60 entrées) | `SinaiaFrame.cpp:408-524` ; table de 142 `EVT_*` `:168-309` | `wxMenuBar`, `EVT_MENU`, accélérateurs encodés dans le libellé `:411-414` | **sans équivalent** |
| 2 | Docking / perspectives (9 panneaux) | `m_mgr` `SinaiaFrame.h:432` ; `AddPane` ×9 ; `SavePerspective:1051` ; `LoadPerspective:1297` | **wxAUI** — 121 occurrences de `wxAui*` | **sans équivalent** |
| 3 | Onglets de documents | `CreateNotebook:3471-3490` ; `AddPage` ×7 ; `OnNotebookPageChanged:1665-1754` | `wxAuiNotebook` | **à réécrire, structurant** (cf. §1.3) |
| 4 | Vue 3D | `MyGLCanvas` ctor `wxOpenGLCanvas.cpp:121-179` ; contexte racine partagé `SinaiaFrame.cpp:996-1015` | `wxGLCanvas`, `wxGLContext` | **à réécrire** (glfw/SDL) |
| 5 | Interaction caméra | `OnMouse:1693-1831`, `OnKeyDown:1833-1887` | `wxMouseEvent`, `wxKeyEvent` | **trivial** — la loi vit dans `Ctrackball` |
| 6 | Panneau « Models » | `UpdateModelsList:1981-2060` ; `BuildModelsIcons:1934-1979` | `wxScrolledWindow`, `wxBitmapButton`, `wxMemoryDC` | **à réécrire — cas le plus favorable** |
| 7 | Panneau « Model information » | `UpdatePropertiesGrid:2172-2470` (**299 l.**) ; `MakeColorSwatch:76-100` | `wxPropertyGrid`, `wxTreeCtrl`, `wxImageList` | **à réécrire** — favorable |
| 8 | Panneau « Parameters » | `PropertyPanel.h:16-52` ; `OnParameterChanged:2875-2929` | `wxPropertyGrid` | **à réécrire, modèle déjà neutre** |
| 9 | Dock « Treatments » | `:803-862` ; `CurvaturePanel.h:24-53` ; `DecimationPanel.h:18-51` | `wxAuiNotebook`, `wxComboBox`, `wxSlider` | **à réécrire, trivial** |
| 10 | Barre d'outils (13 outils) | `:553-603` ; synchro « sticky » à **3 endroits** `:1685-1725`, `:1886-1920`, `:3015-3282` | `wxAuiToolBar`, `wxChoice`, `wxArtProvider` | **à réécrire** — la triple synchro est de la dette qui disparaît |
| 11 | Dialogues de fichier | `OnOpen:1779-1792` … `wxDirDialog:1496,:1536` | `wxFileDialog` ×11, `wxDirDialog` ×3 | **sans équivalent — brique tierce obligatoire** |
| 12 | Modales + sélecteur de couleur | `wxMessageBox` ×11 ; `wxGetColourFromUser:3321` | divers | **à réécrire**, sauf le sélecteur de couleur : **brique tierce** |
| 13 | Explorateur de fichiers | `m_dcDirectory:884-894` ; `PopulateFilesList:1396-1411` ; catalogue `SupportedFormats.h:48-119` | `wxGenericDirCtrl`, `wxListCtrl` | **sans équivalent** — à réécrire sur `std::filesystem` |
| 14 | Favoris + dernier répertoire | `LoadFavorites:1564-1576` … `SaveLastDir:1595-1601` | **`wxConfig`** (16 occ.) → registre Windows | **à réécrire** — `extern/nlohmann` déjà vendoré |
| 15 | Glisser-déposer | `ModelDropTarget` `wxOpenGLCanvas.cpp:32-84` ; `DropModelFiles:1842-1860` | `wxDropTarget`, `wxDataObjectComposite` | **sans équivalent** ; le dépôt OS est couvert par glfw, l'interne non |
| 16 | Journalisation | `Log:2472-2475` ; puits posé `:977` | `wxTextCtrl` | **trivial** — `cgre::LogSink` est déjà un `std::function` (`diagnostics.h:38-42`) |
| 17 | Barre d'état + FPS | `CreateStatusBar:526-529` ; `wxOpenGLCanvas.cpp:1615-1630` | `wxStatusBar` | **trivial** |
| 18 | Icônes fenêtre / exécutable | `:393-401` ; `sinaia.rc` ; `CMakeLists.txt:51-53` | `wxIconBundle` | **à réécrire** (`glfwSetWindowIcon`) ; le `.rc` survit tel quel |
| 19 | Export d'image / capture | `OnExportImage:2524-2582` ; `SaveScreenshot` `wxOpenGLCanvas.cpp:1633-1674` | `wxImage` (13 occ.) | **trivial** — `extern/stb` fournit `stb_image_write` |
| 20 | Console distante (47 commandes, port 7777) | `SinaiaApp.cpp:31` ; catalogue `RemoteConsole.cpp:1491-1597` | **8 sites d'appel wx seulement** | **quasi trivial** (cf. §1.4) |
| 21 | Réglages applicatifs | `OnSettings:2936-2958` ; `SettingsDialog.h` (348 l.) | `wxDialog`, `wxColourPickerCtrl` | **à réécrire** — dont la moitié (`notebookStyle`, `notebookTheme`) **n'a plus d'objet hors wx** |
| 22 | Internationalisation | 137 occurrences de `_()` | **aucun** : `wxLocale` = 0, aucun `.po`/`.mo` | **rien à porter** : seule la macro existe |

**Services wx mesurés comme non utilisés** (`grep -rho … | wc -l` sur `sinaia/*.{cpp,h}`) :
`wxTimer` **0**, `wxThread` **0**, `wxClipboard` **0**, `wxPrint*` **0**, `wxLocale` **0**.
Impression, presse-papiers et temporisation **n'existent pas** dans `sinaia` : ils ne coûteront rien
à porter, et l'absence d'impression dans RmlUi est sans objet.

### 1.2 Quantification — et pourquoi le chiffre est trompeur

| Mesure | Instrument | Valeur |
|---|---|---|
| Lignes totales (28 fichiers réels, hors `icons.h`/`*.xpm`) | `cat … \| wc -l` | **11 590** |
| Lignes contenant `wx` | `grep -c "wx"` | **1 702** (14,7 %) — *minorant* |
| Lignes `wx` **ou** membre de type wx **ou** `EVT_*` | motif étendu | **2 158** (18,6 %) — *majorant* |
| idem, `SinaiaFrame.cpp` seul | | **1 244 / 4 326** (28,8 %) |
| idem, `wxOpenGLCanvas.cpp` seul | | **108 / 2 317** (4,7 %) |
| `RemoteConsole.cpp` | `grep -c "wx"` | **17 / 1 774** (1,0 %), dont **8 sites réels** |

**≈ 1 600 à 2 150 lignes relèvent de wx, soit 14 à 19 %.** Ce chiffre est exact et **il ne faut pas
s'y fier**, pour deux raisons :

1. Les deux instruments ratent **par construction** le couplage structurel : `MyFrame : wxFrame` et
   `MyGLCanvas : wxGLCanvas` rendent tout le fichier dépendant de wx, quel que soit le compte de
   lignes. Un inventaire par nom ne voit pas l'héritage.
2. Les 81 % restants **ne sont pas réutilisables tels quels**. Dans `SinaiaFrame.cpp`, la partie
   véritablement neutre est : les 12 gestionnaires de traitements (`:3721-4126`, ≈ 405 l., moins
   leurs `*m_pWndLogging <<`), `ReplaceMeshGeometry:3867-3893` (27 l., zéro ligne wx),
   `colorizeCurvatureMeshes:4157-4165` et `restoreCurvatureColors:4169-4178` (22 l.), et la table
   `CASE(…)` de création paramétrée `:2639-2663` (25 l.).
   **Soit ≈ 480 lignes réellement neutres sur 4 326 — 11 %.** Le reste est du câblage, ou du métier
   *imbriqué* dans du câblage : les 299 l. de `UpdatePropertiesGrid` sont à 90 % du formatage de
   chaînes dans un `wxPropertyGrid`.

### 1.3 Le fait structurant : `MyGLCanvas` **est** le document

`MyGLCanvas` (2 317 l.) n'est pas une vue. C'est le document, la caméra, le picking et
l'instrumentation, enfermés dans un widget wx :

| Responsabilité captive | Site |
|---|---|
| **La scène elle-même** (`m_pVModels`, détruite par `~MyGLCanvas:204`) | `wxOpenGLCanvas.h:553` |
| Sélection, survol, **règle de cible** (« un seul modèle → c'est lui ; plusieurs → la sélection, sinon refus ») | `.h:554-556` ; `GetTargetModel:243-255` ; `IsTargetAmbiguous:263-266` |
| Transformation de normalisation figée | `.h:386-388` ; `ApplyNormalization:358-428` |
| Loi de cadrage caméra | `FrameCamera:758-815` ; `UpdateSceneSpheres:820-855` |
| Picking (calcul pur, zéro ligne wx) | `ScreenToRay:953-967` ; `PickModel:969-1026` ; `rayAabb:912-935` |
| `SceneSignature` (FNV-1a, zéro ligne wx) | `:860-877` |
| Instrumentation caméra (faite pour être pilotée par la console) | `GetCameraInfo:528-550` ; `ProjectPoint:582-631` ; `UnprojectPixel:1073-1082` |
| Ouverture / ajout / rechargement de fichiers | `LoadModel:1087-1168` … `ReloadModel:1265-1319` |

⚠ **Le couplage inverse est le pire signal** : le canvas remonte à la frame par
`dynamic_cast<MyFrame*>(wxGetTopLevelParent(this))` **six fois** — `wxOpenGLCanvas.cpp:51, 327, 1615,
1703, 1772, 1817` — pour appeler `HighlightModelRow`, `RefreshModelSelection`, `IsShowFps`,
`GetStatusBar`, `DropModelFiles`. Vue et fenêtre sont mutuellement dépendantes.
**Un portage qui recopie cette structure recopiera ce cycle.**

**Absent, et c'est à noter** : aucun historique / undo / redo.
`grep -rni "undo\|redo\|history" sinaia/*.{cpp,h}` → **zéro résultat**, corroboré par la lecture
intégrale des deux gros fichiers. Rien à porter de ce côté — mais rien non plus pour amortir un
portage : pas de patron Commande à réutiliser.

### 1.4 Ce qui est déjà portable

- **`cgre` n'a aucune dépendance wx** : `grep -rn "wx" src/cgre/*.{h,cpp}` → **6 occurrences, toutes
  dans des commentaires** (`capabilities_manager.cpp:20`, `diagnostics.cpp:70`, `diagnostics.h:11`,
  `mesh_renderer.h:15`, `surface_program.cpp:103`, `widgets_renderer.h:7`). Aucun `#include <wx/…>`.
  Aucune création de fenêtre ni de contexte (`grep -rln "glfw\|wglMakeCurrent\|CreateWindow"` →
  aucun fichier). Le module se bâtit et se teste déjà sans wx : `ENABLE_CGRE` est une option distincte
  de `ENABLE_SINAIA` (`CMakeLists.txt:157-164`) et `ci-linux` l'active (`CMakePresets.json:50`).
- **Les traitements sont déjà hors UI** : `MeshAlgoSmoothing{Taubin,Laplacian}`,
  `MeshAlgoSubdivision{Loop,Karbacher,Sqrt3}`, `MeshAlgoTensorEvaluator`, `Normals::EvalOnVertices`,
  `Mesh_half_edge::simplify`, `VMeshesIO::{load,save}` vivent tous dans `cgmesh`. **Ce n'est pas là
  qu'est le travail.**
- **`RemoteConsole` est la pièce la plus portable** : sur 1 774 lignes, **~15 sont à toucher**.
  `wxTheApp->CallAfter` (`:44`, dans le template `callOnMain:39-53`) est le **seul point
  architectural** — il faut une file de tâches vers la boucle de rendu ; `glfwPostEmptyEvent` plus un
  `std::queue<std::function<void()>>` avec mutex suffisent. Les 7 autres sites sont
  `wxString::FromUTF8` ×4 et `wxFileExists` ×3.
  ⚠ Mais **`cgnet` n'est bâti que sous `WIN32`** (`CMakeLists.txt:247-249`) : `peles` sous Linux
  n'aurait pas de console.
- **Le dialogue de fichier a un précédent interne réutilisable tel quel** :
  `vecna/src/Core/FileDialog.cpp:34` utilise **portable-file-dialogs**, tiré par `FetchContent`
  (`vecna/cmake/FetchDependencies.cmake:78-94`). Son interface publique est en
  `std::filesystem::path` / `std::optional`, sans dépendance toolkit (`FileDialog.hpp:11-33`).

### 1.5 Code mort à ne pas porter — ≈ 1 210 lignes (10 %)

| Élément | Lignes | Preuve |
|---|---|---|
| `main.cpp` (« Hello World » avec un `MyFrame` concurrent) | 74 | **absent des sources** — `sinaia/CMakeLists.txt:27-40` |
| `Managers.{h,cpp}` | 58 | corps **intégralement en commentaire** ; compilés quand même |
| `DrawingArea.{h,cpp}` | 120 | l'en-tête le dit : « ⚠ CODE MORT » (`DrawingArea.h:9-10`) |
| `wxSizeReportCtrl.{h,cpp}` | 91 | appelants uniquement dans des blocs commentés `:658-690`, `:940-949` |
| `CreateGrid` + `CreateHTMLCtrl` + `GetIntroText` (« Welcome to wxAUI ») | ~110 | appelants dans le bloc commenté `:951-963`. ⚠ Le composant **`html` est pourtant lié** (`CMakeLists.txt:72`) et **sa DLL déployée** (`:94`) |
| `SettingsPanel.{h,cpp}` | 310 | instanciation unique `:946`, dans un bloc commenté |
| Gestionnaires vides ou commentés | ~110 | `OnButtonSmoothingTaubin:3561-3574`, `OnSlider:3666-3684`, `UpdateGeometry:3332-3381`… |
| Blocs commentés de `wxOpenGLCanvas.cpp` | ~340 | `:1483-1596`, `:2219-2317` |

⚠ **Piège armé** : `UpdateGeometry:3332-3381` déréférence `m_pRadioGeometries[i]` (`:3341`), membre
**jamais initialisé** (`SinaiaFrame.h:421`, aucun `new` dans le constructeur). Les
`EVT_RADIOBUTTON:252-259` ne peuvent pas se déclencher, les boutons n'existant pas — mais le code est
là. **À ne pas recopier.**

---

## 2. Ce que fournit RmlUi

*Toutes les données de ce chapitre sont sourcées sur la documentation et le dépôt GitHub du projet,
consultés le 2026-09-22. Rien n'a été vérifié par exécution locale.*

### 2.1 Identité

| | |
|---|---|
| Dernière version | **6.3**, publiée le **2026-08-22** ; `master` porte déjà `6.4` en pré-version |
| Rythme | **3 releases mineures sur 24 mois** (6.1 en 2025-04, 6.2 en 2026-01, 6.3 en 2026-08) |
| Licence | **MIT**, sans piège. Dépendances embarquées : robin-hood (MIT), polices du debugger (SIL OFL), SDL_shadercross (Zlib). **Aucune contamination copyleft** |
| C++ | **C++17 requis depuis la 6.0** — compatible avec la cible du dépôt |
| CMake | ≥ 3.10 (≥ 3.16 si `RMLUI_PRECOMPILED_HEADERS`, ON par défaut) |
| Compilateurs | MSVC, GCC, Clang, couverts par CI GitHub Actions + AppVeyor |
| Dépendances | **FreeType seule**, et remplaçable (`RMLUI_FONT_ENGINE=none`). Optionnelles toutes OFF : Lua, Lottie, SVG, HarfBuzz |
| Vendorisation | `find_package` après install, **vcpkg**, **Conan**. `FetchContent` et `add_subdirectory` **ne sont pas documentés** (0 occurrence dans `building_with_cmake.md`), mais `CMakeLists.txt:13-15` définit `RMLUI_IS_ROOT_PROJECT` : le projet est **écrit** pour l'inclusion en sous-répertoire. *(inféré — à valider par un essai)* |
| Gouvernance | **298 commits sur 24 mois, dont 221 par le mainteneur = 74,2 %**. Deuxième contributeur : 8 commits. **Facteur de bus = 1**, et le projet l'assume : *« developed purely on a volunteer-basis… with no commercial aspect »* (`AI_POLICY.md`) |

**L'intégration technique dans ce dépôt ne pose aucun problème structurel.** C++17, CMake, MSVC+GCC,
licence MIT : tout coche.

### 2.2 Modèle d'intégration

Cinq interfaces, dont **une seule obligatoire** :

| Interface | Statut |
|---|---|
| `RenderInterface` | **Obligatoire** (ou reprendre un renderer fourni) |
| `SystemInterface` | Défaut fourni ; à surcharger en pratique (horloge, log, **presse-papiers système**, curseur, i18n) |
| `FileInterface` | Défaut stdlib |
| `FontEngineInterface` | Défaut FreeType |
| `TextInputHandler` (IME) | Fourni par le *platform*, ou vide |

Piège de cycle de vie documenté : RmlUi prend des **pointeurs non-propriétaires** sur les interfaces ;
elles doivent survivre jusqu'après `Rml::Shutdown()`.

**Le projet qualifie lui-même ses briques, et la distinction est décisive** :
> *« The provided **renderers and platforms** are intended to be usable as-is by client projects
> without modifications… The provided **backends** on the other hand are not intended to be used
> directly by client projects, but rather copied and modified as needed. »*

`RmlUi_Renderer_GL3` (« Reference implementation ») et `RmlUi_Platform_*` sont destinés à la
production ; les `RmlUi_Backend_*.cpp` — la colle fenêtre + boucle — sont des **échantillons à
recopier**.

⚠ **Couverture des plateformes livrées** : `X11` → pas de HiDPI, pas d'IME. `Wayland` → **pas de
presse-papiers**, pas de HiDPI. `Win32` → tout sauf le tactile. `SDL 3` → tout.
**Si Linux + HiDPI est visé, SDL 3 est le seul chemin complet livré.**

### 2.3 Couverture fonctionnelle — le point décisif

| Besoin de `sinaia` | RmlUi | Détail |
|---|---|---|
| Fenêtre, boucle, contexte GL | **Non** | RmlUi ne crée aucune fenêtre. Backend à recopier |
| **Barre de menus** | **Non** | Absente de l'index exhaustif des éléments |
| **Menus contextuels** | **Non** | idem |
| **Accélérateurs clavier** | **Non** | Pas de table d'accélérateurs |
| **Docking / panneaux détachables** | **RIEN** | cf. encadré ci-dessous |
| **Grille de propriétés** | **Non** | À écrire intégralement |
| **Arbre** | **Non** | À écrire |
| **Dialogues natifs** (fichier, couleur, répertoire) | **Non** | Brique tierce obligatoire |
| **Infobulles** | **Non** | 0 occurrence de `tooltip` dans la doc |
| **`input type=color`** | **Non** | La liste des `type` est close : `text, password, radio, checkbox, range, button, submit` |
| **`select` en list box** | **Non** | Dropdown seul ; issue **#318 ouverte depuis 2022-05-24** |
| **Liste virtualisée** | **Non** | Aucune trace |
| **Sélection de texte hors champs de saisie** | **NON** | cf. encadré |
| Accessibilité, impression | **Non** | Aucune API. *Sans objet ici : `sinaia` n'imprime pas et n'a pas d'i18n active* |
| `input` text/range/checkbox/radio, `textarea`, `progress`, scrollbars | **Oui** | |
| `tabset` / `tab` / `panel` | **Oui** | ⚠ incompatibilité documentée avec `data-for` |
| `handle` (drag / resize) | **Oui** | `move_target`, `size_target`, `edge_margin` — **la seule primitive fournie pour bâtir des panneaux** |
| Table | **Oui, via RCSS** | `display: table`, `colspan`, `rowspan`. Limites : `table-layout: fixed` seulement, pas de *shrink-to-fit* |
| Data binding (MVC) | **Oui**, borné | cf. §2.4 |
| Drag & drop **interne** | **Oui** | `dragstart/drag/dragover/dragdrop/dragend` |
| Drag & drop **OS → app** | **Non** | À fournir par la plateforme |
| Presse-papiers système | **Interface seulement** | Le défaut ne copie qu'**en interne à l'application** |
| Unicode UTF-8, polices de repli, `@font-face` (6.3) | **Oui** | Shaping complexe = HarfBuzz, **en *sample* uniquement** |
| **Rendu à la demande** (CPU au repos) | **Oui**, documenté | `Context::RequestNextUpdate` — **atout net face à imgui** |
| Débogueur d'UI en direct | **Oui** | `Rml::Debugger` |
| Rechargement à chaud .rml/.rcss | Crochets fournis | La surveillance du système de fichiers est à écrire |
| Style : flexbox, animations, transitions, variables, `box-shadow`, filtres | **Oui, excellent** | **C'est ce qui justifie le motif retenu** |

> **Encadré — Docking : il n'y a rien, et ce n'est pas un défaut d'instruction**
>
> - Recherche GitHub issues `repo:mikke89/RmlUi docking` → **total_count = 0**. `dock in:title` → **0**.
> - `grep -i "dock"` sur `rml/controls.md`, `cpp_manual/element_packages.md`,
>   `rcss/user_interface.md` → **0 occurrence**.
> - L'index des éléments RML est **exhaustif et généré par script** ; il liste exactement :
>   `rml, head, title, link, style, body, br, handle, img, form, input, textarea, select, option,
>   label, tabset, tab, panel, progress` (+ `script`, `lottie`, `svg` par plugin). **C'est tout.**
> - Le fil communautaire des ~20 intégrations tierces ne contient que des *backends* de moteurs
>   (Ogre, bgfx, Godot, Unreal, raylib…) et des bindings Lua. **Aucune bibliothèque de widgets.**
>
> La seule primitive est `<handle>`. Zones d'accueil, prévisualisation de dépôt, détachement,
> sérialisation de disposition, onglets déplaçables : **tout est à écrire.**

> **Encadré — Sélection de texte : le point le plus gênant pour un outil de production**
>
> Issue **#661 « Selectable text outside of inputs/textareas »**, **ouverte depuis 2024-08-28**.
> Le mainteneur, le lendemain : *« You're right, we don't have a way to select elements or text
> outside of input fields… all our selection logic is located in the text input widget »*, puis
> *« I would put that in the relatively rare, non-crucial bucket. »*
> Aucune mention de `user-select` dans le `changelog.md` des 6.x.
>
> **Conséquence concrète pour `peles`** : on ne pourra pas sélectionner-copier un nombre de
> triangles, un chemin de fichier ou un message d'erreur affiché dans un panneau.
> `sinaia` le permet aujourd'hui partout. **C'est une régression frontale, à assumer explicitement.**
> Contournement connu : placer le texte concerné dans des `<textarea readonly>`.

### 2.4 Data binding — maturité et limites déclarées par le projet

Le mécanisme est réel et utilisé en production. Les limites sont documentées **par le projet
lui-même** :

- *« removing an element inside a `data-for` view is **undefined behavior and may lead to a crash** »* ;
- *« **only top-level data variables can have a dirty state** »* — on ne peut pas salir un seul index
  de tableau ni un seul membre de structure ;
- *« Adding `data-` attributes after the element has been attached… has **no effect** »* ;
- *« Registering `const` objects or member functions, or **members inherited from a parent class**,
  is not supported »* → **un DTO de présentation sera probablement nécessaire** pour exposer `Mesh` ;
- **incompatibilité nommée avec `<tabset>`, `<panel>` et `<tab>`**, *« notably when combined with the
  `data-for` view »* ⚠ — or ce sont exactement les éléments visés par le besoin « onglets de
  documents » retenu en tête de document.

### 2.5 Ce que le seul précédent desktop connu a réellement coûté

**LichtFeld Studio** (https://lichtfeld.io, GPL-3.0, 3 738 étoiles) est un outil de reconstruction et
d'édition 3D dont *« the whole user interface is made with RmlUi »*. C'est le précédent le plus
direct — et il a **migré depuis Dear ImGui**, migration close en 2026-07 (PR #1395,
`docs/imgui_inventory.md` : *« Dear ImGui and ImPlot are fully removed… RmlUi is the only GUI stack »*).

Volumes mesurés sur leur arbre git :

| Fichier | Taille | Rôle |
|---|---|---|
| `gui/panel_registry.cpp` | 95 393 o | registre des panneaux |
| `gui/rmlui/rml_panel_host.cpp` | 60 803 o | hôte de panneau |
| `gui/panel_layout.cpp` | 58 812 o | calcul du layout + hit-test des splitters |
| `gui/rml_right_panel.cpp` | 30 152 o | dock droit + onglets |
| `gui/rml_bottom_dock.cpp` | 18 655 o | dock bas |
| `gui/layout_state.cpp` | 8 365 o | persistance |
| **≈ 272 000 o** | | **pour un docking à zones fixes, NON détachable** |
| `gui/rml_menu_bar.cpp` | **75 877 o** | barre de menus |
| `gui/global_context_menu.cpp` | 14 902 o | menus contextuels |
| `gui/rmlui/elements/scene_graph_element.cpp` | **154 611 o** | arbre de scène (élément personnalisé) |
| `gui/rmlui/elements/color_picker_element.cpp` | — | sélecteur de couleur (élément personnalisé) |

⚠ **Fait de conception à retenir** : leur viewport 3D **n'utilise pas** `Element::OnRender()`. Ils
calculent le rectangle **à la main en C++** à partir des largeurs de panneaux
(`panel_layout.cpp:1145-1162`, `PanelLayoutManager::computeViewportLayout`). Autrement dit,
**la géométrie du layout RmlUi n'est pas la source de vérité du viewport : elle est dupliquée en
C++.** C'est un coût de conception, pas un détail.

*(Contexte signalé sans surinterprétation : ils utilisent RmlUi **6.2** épinglé, et **Vulkan**, pas
OpenGL.)*

---

## 3. Confrontation : les 22 fonctions face à RmlUi

| Catégorie | Fonctions concernées | Qui fournit |
|---|---|---|
| **Couvert nativement par RML/RCSS** | 9 (Treatments), 16 (log), 17 (barre d'état), et le contenu des panneaux 6, 7, 8 | RmlUi — **et mieux qu'aujourd'hui** : les 46 l. de dessin d'icônes en `wxMemoryDC` (`:1934-1979`) et les 25 l. de pastilles GDI (`MakeColorSwatch:76-100`) disparaissent en RCSS |
| **Trivial, indépendant de RmlUi** | 5 (caméra), 19 (export image → `extern/stb`), 22 (i18n, rien à porter) | le dépôt |
| **Brique tierce, précédent interne existant** | 11 et 12 (dialogues) | **portable-file-dialogs**, déjà utilisé par `vecna` |
| **À écrire, volume modéré** | 3 (onglets), 13 (explorateur sur `std::filesystem`), 14 (favoris → JSON via `extern/nlohmann`), 18 (icônes), 21 (réglages) | `peles` |
| **À écrire, sans précédent** | 1 (menus + accélérateurs), 10 (barre d'outils), 15 (glisser interne panneau→vue) | `peles` |
| **Hors périmètre par décision** | 2 (docking flottant, perspectives) | — |
| **Régression assumée** | sélection-copie de texte partout dans l'UI | — |

---

## 4. Verrous techniques, par ordre de gravité

### V1 — 🔴 `cgre` exige le profil de **compatibilité** OpenGL

Le GLSL de `cgre` est en `#version 120` et lit `gl_ModelViewProjectionMatrix`, `gl_NormalMatrix`,
`gl_LightSource[]`, `gl_FrontMaterial` ; le chemin VBO est alimenté par
`glVertexPointer`/`glEnableClientState`. `src/cgre/surface_program.h:33-38` le dit en clair :
*« C'est un ÉCHAFAUDAGE, pas une cible. Il lie cgre au profil de compatibilité. »*

⚠ **Le précédent interne est un piège** : `cggraph-boilerplate/main.cpp:81-83` demande
`GLFW_OPENGL_CORE_PROFILE` en 3.3 — **incompatible avec `cgre`**. Le copier tel quel donnerait un
écran noir **sans message d'erreur**.

**C'est le premier lot du plan** (§6, L0) : une sonde de ~200 lignes qui prouve que glfw obtient un
contexte de compatibilité sur les pilotes cibles et que `cgre` y dessine. **Si elle échoue, `peles`
sur `cgre` est impossible et tout le reste tombe.**

### V2 — 🔴 `cgre` est **mono-vue par construction**

`MeshRenderer` (`mesh_renderer.h:122, :135-140`), `CapabilitiesManager` (`capabilities_manager.h:7,
:20`), `MaterialRenderer` et `VBOManager` sont des singletons `getInstance()` portant un état
d'identifiants GL global. `debt_cgre.md` : *« une seconde vue 3D ou un rendu hors-écran est
**structurellement impossible** »*. C'est ce qui force déjà `CuttingMat` à une instance par onglet,
*« au prix de 54 Mo de texture dupliqués par onglet »* (`sinaia/CuttingMat.h:45-53`).

**Conséquence directe sur le périmètre retenu** : les **onglets de documents** (fonction 3, conservée)
héritent de cette limite telle quelle. Elle n'est ni aggravée ni résolue par `peles`.

### V3 — 🟠 Collision frontale avec le verdict de `debt_cgre.md`

> *« **Verdict : figer cgre, ne pas le migrer, ne pas le supprimer tant que `sinaia` vit.** Le porter
> vers un pipeline programmable coûterait 15-25 j sans test et sans filet, pour un cul-de-sac. »*
> — `debt_cgre.md` §5

**`peles` en OpenGL/`cgre` contredit ce verdict** : il prolonge la vie de `cgre` et lui ajoute un
second consommateur. **C'est une décision à prendre explicitement, pas à subir** — voir §8, D-6.

Défauts 🔴 ouverts qui seraient **hérités tels quels** :
- `MeshRenderer::RemoveMesh` ne libère **aucun** tampon GL ; `~MeshRenderer` vide et jamais exécuté ;
- `MaterialRenderer` : 256 matériaux en dur, jamais décrémenté, textures jamais supprimées,
  déduplication **par adresse** ;
- `mesh_draw` branche quad : garde `.empty()` manquante sur les UV → `operator[]` sur vecteur vide ;
- `glDrawElements(GL_TRIANGLES, 3*GetNFaces(), …)` sur source triangulée.

### V4 — 🟠 Le renderer GL3 de RmlUi ne restaure pas tout l'état OpenGL

Lecture de `Backends/RmlUi_Renderer_GL3.{cpp,h}` : `BeginFrame()` (l. 813-894) sauvegarde l'état dans
`GLStateBackup`, `EndFrame()` (l. 897-971) le restaure. **Bien mieux que la moyenne**, mais **ne sont
ni sauvegardés ni restaurés** : `GL_FRAMEBUFFER_BINDING`, `GL_CURRENT_PROGRAM`,
`GL_VERTEX_ARRAY_BINDING`, les *bindings* de texture, et **`GL_FRAMEBUFFER_SRGB`** — ce dernier étant
désactivé en `BeginFrame` (l. 874) et **jamais réactivé**. `EndFrame()` fait
`glBindFramebuffer(GL_FRAMEBUFFER, 0)` **en dur** (l. 909).

**Hypothèse, non vérifiée par exécution** : cohabiter avec `cgre` demandera de **patcher le
renderer** — ce que le projet autorise et attend explicitement.

### V5 — 🟠 Il n'existe plus aucun oracle pour comparer `peles` à `sinaia`

Double constat, tous deux vérifiés :
1. l'oracle image n'était **pas reproductible** — `debt_sinaia.md` §2 mesure jusqu'à **24 621 octets
   d'écart entre deux exécutions du même binaire**, sur 9 images sur 18 ;
2. **l'outil qui le produisait n'existe plus** : `sinaia/tools/` a été supprimé le **2026-09-17** par
   le commit **`5a87adb` « cleaning »** — 7 scripts, dont `sinaia-console.ps1`, `shader-captures.ps1`,
   `pbr-captures.ps1`. Vérifié par `git log --diff-filter=D --name-only -- sinaia/tools`.

⚠ **Aucun critère d'acceptation de `peles` ne peut donc s'énoncer en pixels.** Le seul oracle
disponible est **numérique et passe par la console** : `camera show`, `camera project`, `info`,
`mesh N`, `caps` — *« identiques au centième de pixel d'un lancement à l'autre »*
(`debt_sinaia.md:107-109`).

**Conséquence non négociable sur l'ordre des lots** : `peles` doit exposer la console distante **dès
le premier jour**, et le comparateur numérique doit être reconstruit **avant** toute UI.
**Le premier livrable est l'instrument, pas l'interface.**

### V6 — 🟡 Réglages, `<windows.h>`, et divers

- `gl_wrapper.h:5-9` tire `windows.h` puis `glad/wgl.h` sous `_WIN32`. L'ordre d'inclusion est
  contraignant et documenté sur place chez les appelants (`RemoteConsole.cpp:11-13` : « gl_wrapper.h
  AVANT tout en-tête wx » ; `wxOpenGLCanvas.cpp:4` : `#define _WINSOCKAPI_`). Sans wx, la contrainte
  s'allège mais ne disparaît pas.
- **La moitié de `SettingsDialog.h` n'a plus d'objet** : `notebookStyle` et `notebookTheme`
  (`PanelSettings.h:22-23`) règlent l'apparence de wxAUI.
- `SupportedFormats.h` est **déjà factorisé** mais **typé `wxString`/`wxChar`** (`:10-11, :25-27,
  :77-119`) : à reporter sur `std::string`.
- ⚠ Incohérence de configuration relevée au passage : `CMakeUserPresets.json:13` et
  `CMakeSettings.json:36` donnent **deux chemins différents** pour la même wxWidgets 3.3.2.

### V7 — 🟡 Ce que le déploiement **ne** gagnera **pas**

| Bloc | Aujourd'hui | Sous `peles` |
|---|---|---|
| DLL wxWidgets | **6** (`core`, `base`, `aui`, `html`, `gl`, `propgrid`) | **0** ⚠ dont `html`, déployée pour du **code mort** (§1.5) |
| DLL OCCT | **~48** (chiffre du commentaire `sinaia/CMakeLists.txt:118`, **non recompté**) | **~48, inchangées** — c'est `cgmesh` qui en dépend, pas l'UI |
| OpenNURBS, lib3mf | 2 | 2 |
| RmlUi + FreeType + glfw/SDL | 0 | **ajoutées** |

**Gain net : −6 DLL sur ~56, soit ≈ 11 %, et probablement annulé par les dépendances entrantes.**

⚠ **« Alléger le déploiement » n'est pas un argument valable pour `peles`.** De même,
« multiplateforme » ne l'est pas : `sinaia` est déjà annoncé MSVC/Windows **et** GCC/Linux et
`sinaia/CMakeLists.txt:145-149` porte une branche `UNIX`. wx n'est pas le frein.
**Le motif retenu — style et thématisation — est le seul des quatre examinés qui résiste.** Il est
recevable : RCSS donne des feuilles de style, des animations, des transitions et un rechargement à
chaud que ni wxWidgets ni Dear ImGui ne donnent.

---

## 5. Pistes explorées

Les six pistes ont été instruites avant convergence, conformément au §3 du skill
`feasibility-analysis`. Elles sont toutes consignées ici, **retenue comme écartées**.

### Piste 1 — Portage « big bang » : RML/RCSS sur glfw + OpenGL ✅ **RETENUE**

**Principe.** Nouvelle cible `peles` : hôte glfw, contexte OpenGL de **compatibilité**,
`RenderInterface`/`SystemInterface` implémentés sur `cgre`/glad, toute l'UI décrite en RML/RCSS.
`sinaia` reste intact.

**Ancrages.** `cggraph-boilerplate/main.cpp:77-164` pour l'ossature glfw (**en changeant `:81-83`**) ;
`wxOpenGLCanvas.cpp:1350-1600` (`DrawGL`), qui n'appelle **aucune** fonction wx entre `:1354` et
`:1598` sauf `SwapBuffers():1599` ; `vecna/src/Core/FileDialog.{hpp,cpp}` copié tel quel ;
`RemoteConsole.cpp` moins 8 sites ; `extern/nlohmann` à la place de `wxConfig`.

**Trade-offs.** *Coût* : 28–58 j (§7). *Risque* : concentré sur les fonctions les moins
intéressantes (menus, explorateur, glisser interne) plutôt que sur le métier. *Réutilisation* :
**nulle**, et **négative** dans l'autre sens — toute correction devra être faite deux fois.
*Impact sur `sinaia`* : **nul**, et c'est son principal mérite.

**Ce qu'elle ne résout pas.** Aucune dette du §4. `cgre` reste, avec ses 4 🔴. Elle crée la
**quatrième** couche application du dépôt.

### Piste 2 — Extraction préalable d'un noyau `cgapp`, puis façade RmlUi ❌ écartée

**Principe.** Extraire d'abord `src/cgapp` (Document, SceneView, Treatments, Settings, FormatCatalog,
Console) de `sinaia`, **refactorer `sinaia` dessus**, puis écrire `peles` comme seconde façade.
~900 l. dont **~50 seulement mentionnent `wx`** sont candidates (`wxOpenGLCanvas.cpp:243-266,
358-476, 651-906, 912-1082, 1087-1345` et `SinaiaFrame.cpp:3721-4126, 3867-3893, 4157-4178`).

**C'était la recommandation de l'agent `architect`**, pour trois raisons qu'il faut consigner
honnêtement : elle ferme la duplication au lieu de l'aggraver ; son premier lot a un critère
d'acceptation falsifiable **sans écrire une ligne d'UI** ; et son étape 1 a une valeur indépendante de
`peles` (testabilité en CI Linux d'une couche qui n'a aucun test aujourd'hui).

**Raison de l'écart** : elle impose de **toucher `sinaia`**, donc de risquer une régression sur
l'application qui marche, **sans aucun test pour la couvrir** — c'est son lot le plus risqué (4–7 j).
Elle coûte **16–28 j avant que quoi que ce soit de visible n'existe**, ce qui est incompatible avec un
motif d'exploration visuelle. Et son état intermédiaire est le plus coûteux de tous (règle
« Transformations en Masse » du skill `global-context`).
⚠ **Son argument de fond reste valide et non réfuté** — voir la fin du §8.

### Piste 3 — Coquille hybride : dialogues natifs, RmlUi pour les panneaux seulement ❌ écartée

**Principe.** glfw + portable-file-dialogs + RmlUi pour les 5 panneaux uniquement (≈ 620 l. de
construction impérative : `SinaiaFrame.cpp:693-727`, `:2172-2470`, `:1981-2060`, `:734-736`,
`:803-862`, `:2472-2475`), **sans aucun onglet ni découpage dynamique**.

**Raison de l'écart** : elle livre strictement moins que `sinaia` et, la décision « onglets oui »
étant prise, son économie principale disparaît. Elle reste la moins chère des portages (18–38 j) si
le périmètre devait être réduit en cours de route — **à garder comme repli**.

### Piste 4 — Dear ImGui plutôt que RmlUi ❌ écartée

**Principe.** Même coquille glfw + OpenGL de compatibilité, UI en Dear ImGui — **déjà vendoré**
(`extern/imgui-1.89.9`, `IMGUI_DIR` `CMakeLists.txt:190`), avec `imgui-node-editor-0.9.3`.

**Ce qui plaidait pour elle, et qu'il faut consigner :**
- **Trois précédents internes qui fonctionnent** : `cggraph-boilerplate/main.cpp` (natif glfw),
  `maker/graph_editor.cpp` (WASM, dans un Web Worker, sans `imgui_impl_glfw`), `vecna` (Vulkan).
  Aucune inconnue.
- ImGui **fournit** ce que RmlUi ne fournit pas : docking (branche `docking`), fenêtres, menus
  (`BeginMenuBar`), arbres (`TreeNode`), tables, sliders, combos. **Les fonctions 1, 2, 3, 6, 7, 8,
  9, 10, 16, 17 sont des primitives.**
- Zéro nouvelle dépendance, zéro nouveau déploiement. **La moins chère de toutes les pistes qui
  livrent une UI : 15–32 j.**

**Raison de l'écart, et elle est directe** : ImGui est **immédiat**, donc la construction de l'UI
reste **impérative** — on remplace 620 lignes de `new wxButton` par 620 lignes de `ImGui::Button`.
Le thème est du code, pas une feuille de style. **Le gain structurel est faible, et le motif retenu
est précisément celui qu'ImGui ne sert pas.** Il va par ailleurs à contre-courant du rendu à la
demande, que RmlUi documente.
*(À charge, pour ne pas biaiser : le docking d'ImGui vit sur une branche non fusionnée depuis 2020.
Ce n'est pas un argument en faveur de RmlUi, qui n'a rien du tout.)*

### Piste 5 — Ne pas faire ❌ écartée

**Principe.** Refuser le portage ; corriger les 5 défauts de `debt_sinaia.md` (dont le n°1, correctif
d'une ligne) et extraire `cgapp` **sans** seconde façade. 6–12 j, **zéro interface livrée**.

**Raison de l'écart** : le motif retenu est l'apparence ; cette piste n'y répond pas.

### Piste 6 — Étendre `sulina` plutôt que créer `peles` ❌ écartée

**Principe.** `sulina` **est déjà** une application de ce dépôt avec une UI déclarative (QML,
2 243 l.) et un moteur moderne (Vulkan via `cgre2`), et `cgre2` est la couche que `debt_cgre.md`
désigne comme la bonne direction.

**Raisons de l'écart** : Qt 6.11 + SDK Vulkan en dépendances de build (`sulina/CMakeLists.txt:8, 14,
16-25`), déploiement bien plus lourd que wx ; `CgreQuickItem.h:249-250` porte du code **non compilé
depuis le 2026-08-17** ; `sulina` ne connaît ni les onglets multi-documents, ni les traitements, ni
le tapis de coupe. ⚠ **Non chiffrée** : `CgreQuickItem.cpp` (1 324 l.) et le QML n'ont pas été lus.
Déclarée non estimée plutôt que devinée.

---

## 6. Plan incrémental sur la piste retenue — MVP → cible

**Principe d'ordonnancement** : chaque lot a un critère d'acceptation **falsifiable** et, autant que
possible, **numérique**. Les deux premiers lots ne produisent **aucune interface** : ce sont les
instruments sans lesquels rien de ce qui suit n'est mesurable (§4, V5).

### Phase 0 — Instruments (préalable, non optionnel)

| Lot | Contenu | Critère d'acceptation |
|---|---|---|
| **L0** | **Sonde de contexte** : glfw + profil de **compatibilité** + `cgre` dessinant un cube texturé, sans aucune UI. ~200 l. | `glGetString(GL_VERSION)` rapporte un contexte de compatibilité sur les pilotes cibles **et** un `glBegin` passe. ⚠ **Si ce lot échoue, `peles` sur `cgre` est impossible** — arrêt et réouverture de la décision D-6 |
| **L0b** | **Reconstruction du comparateur numérique** sur la console distante (remplace ce que `5a87adb` a supprimé) : balayage d'azimuts via `camera project`, `info`, `mesh N`, `caps` | Deux exécutions de `sinaia` rendent des sorties **identiques octet pour octet**. C'est la référence à laquelle `peles` sera comparé |

### Phase 1 — MVP : la vue, la console, aucun chrome

| Lot | Contenu | Critère d'acceptation |
|---|---|---|
| **L1** | **Hôte `peles`** : fenêtre glfw, boucle, `RenderInterface` + `SystemInterface` RmlUi sur glad, `Rml::Debugger` activé | Une fenêtre s'ouvre, un document RML vide se rend par-dessus un fond `cgre` |
| **L2** | **Portage du cœur de vue** : `DrawGL`, caméra, picking, document — recopiés de `MyGLCanvas` et **détypés de wx** (~900 l. déplacées, ~50 à toucher). ⚠ **Ne pas recopier le `dynamic_cast<MyFrame*>` ×6** (§1.3) ni le code mort du §1.5 | `peles` charge un OBJ et l'affiche ; orbite, dolly, pan identiques |
| **L3** | **Console distante** : `RemoteConsole` moins ses 8 sites wx, plus une file `std::queue<std::function<void()>>` + `glfwPostEmptyEvent` à la place de `wxTheApp->CallAfter` | **Les 47 commandes répondent.** `peles` et `sinaia` donnent des sorties **identiques** au comparateur L0b sur le même fichier. ⚠ **Windows seulement** : `cgnet` n'est bâti que sous `WIN32` (`CMakeLists.txt:247-249`) |

**Fin de phase 1 = le MVP.** À ce stade `peles` n'a **aucune** interface graphique, et c'est
délibéré : il est **mesurable**, donc comparable, ce qu'aucune capture d'écran ne permettrait.

### Phase 2 — Les panneaux : là où RmlUi gagne

| Lot | Contenu | Critère d'acceptation |
|---|---|---|
| **L4** | **Panneaux RML/RCSS** : Models, Model information, Parameters, Treatments, Logging. Remplace ~620 l. impératives. Les 46 l. de `wxMemoryDC` et les 25 l. de pastilles GDI **disparaissent** en RCSS | Les 13 propriétés, les deux arbres et les 5 traitements sont présents et pilotables. **Le thème est modifiable sans recompiler** |
| **L5** | **Rechargement à chaud** des `.rml`/`.rcss` : surveillance du système de fichiers + crochets RmlUi | Modifier une feuille de style met l'UI à jour sans redémarrer. **C'est le livrable qui justifie le motif retenu** — le sortir tôt |
| **L6** | **Disposition** : zones redimensionnables en flex RCSS + `<handle>`, **onglets de documents**. ⚠ **Pas de flottement, pas de perspectives**. ⚠ Vérifier tôt l'incompatibilité déclarée `<tabset>` × `data-for` (§2.4) | Trois zones redimensionnables ; N documents en onglets, fermables |

### Phase 3 — Le chrome

| Lot | Contenu | Critère d'acceptation |
|---|---|---|
| **L7** | **Menus + accélérateurs + barre d'outils** : remplace `:168-309` (142 `EVT_*`), `:408-524`, `:553-603`. ⚠ **Écrire une table d'action unique** consommée par les trois — c'est ce qui supprime la triple synchronisation « sticky » de `sinaia` (`:1685-1725`, `:1886-1920`, `:3015-3282`). ⚠ Mapping touche→action à écrire **explicitement** : RmlUi *« offers no translation between key input and text input »* et ses `KeyIdentifier` sont calqués sur les `VK_*` Windows — **AZERTY n'est pas gratuit** | Les ~60 entrées de menu et les 13 outils déclenchent les mêmes actions, un seul état par action |
| **L8** | **Dialogues** : portable-file-dialogs (copié de `vecna`), sélecteur de couleur (élément personnalisé — RmlUi n'a pas d'`input type=color`), modales RML | Ouvrir, ajouter, enregistrer-sous, exporter image, choisir une couleur |
| **L9** | **Explorateur de fichiers** sur `std::filesystem` + liste filtrée. ⚠ `SupportedFormats.h` à détyper de `wxString`/`wxChar` (`:10-11, :25-27, :77-119`) | L'arbre de répertoires et la liste filtrée par format fonctionnent |
| **L10** | **Glisser-déposer** : OS→fenêtre par `glfwSetDropCallback` ; interne panneau→vue par les événements `drag*` de RmlUi. ⚠ **Aucun précédent dans le dépôt** pour l'interne. Conserver la règle unique `DropModelFiles:1842-1860`, modificateur Shift compris | Déposer un `.obj` depuis l'explorateur système **et** depuis le panneau Files |
| **L11** | **Finitions** : export image (`extern/stb`), icône de fenêtre (`glfwSetWindowIcon`, le `.rc` survit), barre d'état, FPS, favoris + dernier répertoire en JSON (`extern/nlohmann`) | Parité fonctionnelle atteinte, hors exclusions ci-dessous |

### Ce que le plan livre — et ce qu'il ne livre pas

**Ne sera pas livré, par décision ou par limite de RmlUi** : docking flottant et perspectives
(décision D-3) ; **sélection-copie de texte hors champs de saisie** (§2.3, régression assumée) ;
accessibilité, impression, i18n (inexistantes dans `sinaia`, donc sans perte) ; console distante sous
Linux (`cgnet` est `WIN32` seulement) ; la moitié de `SettingsDialog.h` qui réglait l'apparence de
wxAUI (sans objet).

---

## 7. Chiffrage

**Hypothèse de calcul, explicite** : une journée-homme = **150 à 250 lignes de C++ neuf et relu**, ou
**400 à 600 lignes de code déplacé sans changement de sémantique**.

⚠ **Cette fourchette est une estimation, pas une mesure.** Elle est calibrée sur deux chantiers
internes dont on connaît le **volume** (`maker` : 2 404 l. de façade C++ + 2 280 l. de JS + 681 l. de
HTML ; `sulina` : 2 413 l. de C++ + 2 243 l. de QML) mais **dont on ne connaît le temps réel ni par
les documents ni par le dépôt**. `camera_refacto.md` le dit lui-même : *« Coût en temps de chaque
étape : non estimé »*. **Les nombres ci-dessous sont des ordres de grandeur, pas un devis.**

| Lot | Contenu | Ordre de grandeur |
|---|---|---|
| L0 | Sonde de contexte de compatibilité | **1 – 2 j** |
| L0b | Comparateur numérique sur la console | **1 – 3 j** |
| L1 | Hôte glfw + backend RmlUi | **3 – 8 j** ⚠ dominé par R-1 |
| L2 | Cœur de vue détypé de wx | **3 – 6 j** |
| L3 | Console distante | **1 – 2 j** |
| L4 | Panneaux RML/RCSS | **5 – 9 j** |
| L5 | Rechargement à chaud | **1 – 2 j** |
| L6 | Zones redimensionnables + onglets (**sans flottement**) | **3 – 6 j** |
| L7 | Menus, accélérateurs, barre d'outils | **3 – 6 j** |
| L8 | Dialogues + sélecteur de couleur | **2 – 4 j** |
| L9 | Explorateur de fichiers | **2 – 4 j** |
| L10 | Glisser-déposer OS + interne | **2 – 5 j** ⚠ sans précédent interne |
| L11 | Finitions | **2 – 3 j** |
| **TOTAL** | | **28 – 58 j** |

**Comparaison, à la même échelle :**

| Piste | Ordre de grandeur | Note |
|---|---|---|
| **1 — RmlUi big bang (retenue)** | **28 – 58 j** | Le périmètre « onglets oui, flottement non » ramène le lot de disposition de *1 – 15 j* à **3 – 6 j** : c'est l'arbitrage le plus rentable de toute l'analyse |
| 2 — noyau `cgapp` puis façade | 35 – 81 j (dont 16 – 28 j avant tout visible) | Plus cher, mais la première moitié a une valeur indépendante |
| 3 — coquille hybride sans onglets | 18 – 38 j | Repli si le périmètre doit être réduit |
| 4 — imgui | 15 – 32 j | La moins chère qui livre une UI ; ne sert pas le motif retenu |
| 5 — ne pas faire | 6 – 12 j | Livre zéro interface |
| 6 — étendre `sulina` | **non chiffré** | Sources non lues, déclaré plutôt que deviné |

**Ce que ces nombres ne disent pas :**
- ils **excluent** la correction des 4 🔴 ouverts de `debt_cgre.md`, que `peles` hérite intégralement ;
- ils **incluent** L0b (1–3 j), qui n'est pas une option : sans oracle, aucun lot n'a de critère
  d'acceptation vérifiable ;
- **L1 est l'incertitude dominante restante** (facteur 2,7 entre ses bornes), et elle dépend
  entièrement de R-1.

---

## 8. Décisions et réserves

### Décisions prises

| # | Décision | Date | Fondement |
|---|---|---|---|
| **D-1** | **Motif de `peles` : style et thématisation.** Les trois autres motifs examinés sont écartés | 2026-09-22 | Arbitrage utilisateur. Cohérent avec §4 V7 : « déploiement » et « multiplateforme » sont **réfutés par la mesure** ; le style est le seul motif que RCSS sert et que ni wx ni imgui ne servent |
| **D-2** | **Piste 1 — big bang.** `sinaia` n'est pas touché | 2026-09-22 | Arbitrage utilisateur, contre la recommandation de l'`architect` (piste 2). Motif : la piste 2 coûte 16–28 j avant tout visible et risque une régression sur `sinaia` sans test pour la couvrir |
| **D-3** | **Onglets de documents : oui. Docking flottant et perspectives : non** | 2026-09-22 | Arbitrage utilisateur. Ramène le lot de disposition de 1–15 j à 3–6 j |
| **D-4** | **Le premier livrable est l'instrument, pas l'interface** : L0 puis L0b avant toute UI | 2026-09-22 | §4 V5 — il n'existe plus aucun oracle, et celui qui existait n'était pas reproductible |
| **D-5** | **La régression de sélection-copie de texte est assumée** | 2026-09-22 | §2.3, issue #661 ouverte. Contournement `<textarea readonly>` là où c'est critique |
| **D-6** ✅ | **`peles` utilise `cgre`** — sortie (a). Le verdict *« figer cgre »* de `debt_cgre.md` §5 est **révisé en connaissance de cause**, et les 4 🔴 ouverts sont hérités | 2026-09-22 | Arbitrage utilisateur : la vue 3D fait partie du lot 1. ⚠ **Rend L0 obligatoire et premier** (§4 V1) |
| **D-7** ✅ | **RmlUi est tiré par `FetchContent`**, pas vendoré sous `extern/` | 2026-09-22 | Arbitrage utilisateur, sur le modèle de `vecna/cmake/FetchDependencies.cmake:78-94` et `src/cgre2/CMakeLists.txt:46-78`. ⚠ Écart assumé à la convention maison (13 dépendances vendorées). ⚠ RmlUi **ne documente pas** `FetchContent` : à valider par un essai réel |

### Périmètre du lot 1, arrêté le 2026-09-22

| # | Décision |
|---|---|
| **P-1** | **Quatre zones** : arbre de répertoires à **gauche**, liste de fichiers filtrée en **bas**, panneaux d'informations à **droite**, **vue 3D `cgre` au centre** |
| **P-2** | **Pas de panneau Treatments.** La fonction 9 du §1.1 et les 12 gestionnaires de `SinaiaFrame.cpp:3721-4126` sortent du périmètre. La fonction 10 (barre d'outils) également |
| **P-3** | **Cible CMake neuve `peles/`. `sinaia/` n'est pas touché** — aucune modification autorisée dans ce lot |
| **P-4** | ⚠ Conséquence de P-1 + D-6 : **le rectangle du viewport 3D doit être résolu explicitement.** RmlUi n'a pas d'élément canvas (§2.3) ; le seul précédent desktop duplique cette géométrie en C++ (§2.5). L'alternative — lire la géométrie résolue d'un `<div id="viewport">` — est à instruire avant de recopier le précédent |
| **P-5** | ⚠ Conséquence de P-3 : `sinaia/SupportedFormats.h` est **typé `wxString`/`wxChar`** (`:10-11, :25-27, :77-119`) et ne peut pas être modifié. La version détypée vit ailleurs — **risque de divergence à nommer explicitement** |

### Décisions **en attente**, à trancher avant de coder

| # | Question | Pourquoi elle ne peut pas être tranchée depuis le code |
|---|---|---|
| ~~D-6~~ | **Tranchée le 2026-09-22** → voir « Décisions prises ». Seule reste ouverte sa condition de survie : **si L0 échoue, D-6 se rouvre** et les sorties (b) `cgre2`+Vulkan ou (c) renoncer redeviennent les seules | — |
| ~~D-7~~ | **Tranchée le 2026-09-22** → `FetchContent`. Reste à lever : l'essai réel, RmlUi ne documentant pas cette voie | — |
| **D-8** 🟠 | **glfw ou SDL 3 ?** glfw est **déjà vendoré** (`extern/glfw-3.3.10`, `GLFW_DIR` `CMakeLists.txt:192`) mais **non bâti** dans `local-windows` (`:300-303`, `ENABLE_CGGRAPH_BOILERPLATE` par défaut `Off`). **SDL 3 est le seul *platform* livré par RmlUi qui couvre HiDPI + IME + presse-papiers sur Linux** ; glfw n'a pas d'IME | Dépend de l'importance donnée à Linux et au HiDPI |
| **D-9** 🟡 | **Faut-il corriger le défaut n°1 de `debt_sinaia.md` avant de copier le code ?** `SceneSignature` n'est pas résistante au remplacement de scène : `AdoptScene:333-356` ne remet **toujours pas** `m_matLevelSignature` (`.h:496`) à zéro — **vérifié dans l'arbre courant**. Correctif d'une ligne, que `debt_sinaia.md:65-66` demande **dans son propre commit** | Le défaut serait **hérité** par L2. Le corriger d'abord dans `sinaia` le supprime des deux applications |

### Réserves ouvertes

| # | Réserve | Sens du biais | Condition de levée |
|---|---|---|---|
| **R-1** | Ce que RmlUi ajoute réellement au déploiement (taille de `rmlui_core`, FreeType) et au temps de compilation. L'archive officielle de 49,85 Mo **n'est pas exploitable** : elle contient statique + dynamique + Debug + Release + en-têtes | **inconnu**, majorant sur V7 et sur L1 | Construire RmlUi 6.3 localement, MSVC et GCC, et mesurer |
| **R-2** | **Le profil de compatibilité OpenGL est-il obtenable via glfw sur les pilotes cibles ?** | **minorant du risque** — si non, `peles` sur `cgre` est **impossible** | **C'est le lot L0.** Non négociable, et en premier |
| **R-3** | Le renderer GL3 de RmlUi cohabite-t-il avec `cgre` ? `GL_FRAMEBUFFER_SRGB`, `GL_CURRENT_PROGRAM`, le FBO et les *bindings* de texture ne sont pas restaurés ; `EndFrame` fait `glBindFramebuffer(0)` en dur | **majorant** du coût de L1 | Levée par L0/L1. Correctif attendu : patcher le renderer, ce que le projet autorise |
| **R-4** | **Aucune donnée de performance chiffrée sur RmlUi n'existe publiquement.** Les benchmarks sont dans `Tests/Source/Benchmarks/` mais **aucun résultat n'est publié**. On ne sait pas ce que coûte un reflow, ni combien d'éléments tiennent à 60 Hz | **inconnu** | Mesurer sur L4 avec le panneau « Model information » réel (13 propriétés + deux arbres) |
| **R-5** | Le coût de synchronisation du data binding sur un modèle de maillage. La granularité de *dirty* est **la variable racine** : salir un champ parcourt toute la structure | **majorant** du coût de L4 | Mesurer sur L4. Repli connu : DTO de présentation |
| **R-6** | glfw délivre-t-il les `release` hors fenêtre ? (défaut n°5 de `debt_sinaia.md` : le canvas ne capture **jamais** la souris — `CaptureMouse`/`ReleaseMouse` : zéro occurrence) | **majorant** du coût — le défaut pourrait se refermer gratuitement | Sonde de quelques lignes, dans L0 |
| **R-7** | « ~48 DLL OCCT » vient du **commentaire** `sinaia/CMakeLists.txt:118`, non recompté | **inconnu**, probablement exact | `ls ${EXTERN_OCCT}/bin \| wc -l` |
| **R-8** | Les 1 230 lignes non lues de `RemoteConsole.cpp` peuvent contenir d'autres usages wx que les 8 sites relevés | **minorant** : le grep sur `wx*` est exhaustif pour les identifiants, mais raterait un type wx atteint par `auto` | Lecture complète avant L3 |
| **R-9** | La faisabilité d'un docking **détachable** en RmlUi. Aucun précédent trouvé ; LichtFeld s'est arrêté aux zones fixes, et on ignore si c'est par choix produit ou par butée technique | **sans objet** tant que D-3 tient | Rouvrir seulement si D-3 change |

### Le constat que cette analyse n'a pas levé

Il est consigné ici parce qu'il survit à l'arbitrage, et qu'il n'est pas réfuté :

> Le dépôt compte déjà **trois couches application disjointes** pour un seul `cgmesh` — `sinaia`
> (`MyFrame` 4 326 + `MyGLCanvas` 2 317), `sulina` (`CgreQuickItem` 1 324 + `MeshModel` 295), `vecna`
> (`Application.cpp` 1 357), soit **≈ 9 600 lignes**, avec **zéro ligne partagée** entre elles
> au-dessus de `cgmesh`. Six fonctions sur les 22 du §1.1 — chargement, cadrage, trackball, picking,
> courbure, dialogue de fichier — sont déjà réimplémentées **trois** fois.
>
> **`peles` sera la quatrième.**
>
> Le dépôt a déjà **mesuré** ce que coûte une fourche : `camera_refacto.md` §2.1 documente
> `Ctrackball` / `cgre2::Trackball` — *« Deux corrections perdues, personne ne s'en est aperçu. Une
> troisième se perdra. »* La décision D-2 accepte ce risque en échange d'un `sinaia` intact.
> **Atténuation minimale à tenir** : tout correctif apporté au cœur de vue de `peles` (L2) doit être
> reporté dans `wxOpenGLCanvas.cpp`, et réciproquement. Sans discipline explicite, la cinquième
> correction se perdra.

---

## 9. Conclusion

**Le portage est faisable.** Il n'a aucun obstacle rédhibitoire connu, sous réserve de R-2 — que le
lot L0 lève ou fait tomber en un à deux jours. `cgre` est déjà sans wx, les traitements sont déjà
hors UI, la console est déjà portable à 15 lignes près, les dialogues ont un précédent interne
réutilisable tel quel, et l'ordre de grandeur est **28 à 58 jours-homme** pour la parité moins les
exclusions du §6.

**Ce n'est pas un remplacement de bibliothèque d'UI.** RmlUi ne fournit ni menus, ni docking, ni
grille de propriétés, ni arbre, ni dialogues, ni infobulles, ni sélecteur de couleur. Choisir RmlUi,
c'est décider d'écrire soi-même la couche widgets de bureau, en utilisant RmlUi comme **moteur de
layout, de style et de rendu**. Le seul précédent desktop connu l'a fait, et livré — au prix d'environ
500 ko de C++ pour ce que wxAUI donne aujourd'hui gratuitement.

**Ce qu'on achète en échange est réel, et c'est exactement le motif retenu** : la présentation sort du
C++. Les 46 lignes de dessin d'icônes en `wxMemoryDC`, les 25 lignes de pastilles GDI, les 620 lignes
de construction impérative de panneaux et la triple synchronisation « sticky » de la barre d'outils
n'ont pas d'équivalent dans une feuille de style — elles disparaissent. Le rechargement à chaud et le
débogueur intégré donnent une boucle d'itération visuelle que ni wxWidgets ni Dear ImGui ne
permettent. **Aucune autre piste examinée ne procure cela.**

**Deux conditions, non négociables :**

1. **L0 et L0b d'abord.** Sans contexte de compatibilité prouvé et sans comparateur numérique
   reconstruit, aucun lot ultérieur n'a de critère d'acceptation vérifiable — les scripts ont disparu
   le 2026-09-17 et celui qui existait n'était de toute façon pas reproductible.
2. **Trancher D-6 (`cgre` ou `cgre2`) explicitement.** Le plan du §6 suppose `cgre` et donc la
   révision du verdict *« figer cgre »*. Subir cette décision au lieu de la prendre, c'est découvrir
   au sixième mois qu'on a prolongé un cul-de-sac.

---

## 10. Handoff

- **Analyse seule.** Aucune implémentation n'est engagée par ce document.
- Toute suite passe par le sous-agent `developer`, puis **`/code-review` obligatoire** (cf. `/dev-task`).
- Les défauts supposés (R-2, R-3, R-6, et le défaut n°1 de `debt_sinaia.md`) passent d'abord par le
  sous-agent `debugger` pour **reproduction**, avant tout correctif.
- Documents liés : `debt_sinaia.md` (5 défauts, dont 3 seraient hérités), `debt_cgre.md` §5 (le
  verdict que D-6 doit trancher), `camera_refacto.md` §2.1 (le coût mesuré d'une fourche),
  `cgmesh_refacto.md` §6.1 (phases 5-6, risque **conditionnel** et non actuel).
- **Les commits sont gérés par l'utilisateur.** Fichier à stager : `peles.md`.

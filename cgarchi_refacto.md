# Extraction du code gothique dans le module `cgarchi`

## 1. Objectif et critère

Créer une bibliothèque `cgarchi` qui contient tout le code gothique et rien d'autre. Ce qui est
générique, ou qui sert ailleurs, reste dans sa bibliothèque d'origine.

Position : `cgmath → cgimg → cgmesh → cgarchi`, au même niveau que `cgre`. `cggraph_nodes` et
`maker` lient `cgarchi`.

Statuts : [V] vérifié dans le code, [H] hypothèse, [M] mesuré.

## 2. Décisions

| # | Sujet | Décision | Date |
|---|---|---|---|
| D1 | `bar_fusion*` (générique dans sa forme, mais seul le gothique s'en sert) | Déplacé dans `cgarchi` | 2026-10-22 |
| D2 | `surface_architecture.{h,cpp}` (mélange de gothique et d'architecture non gothique) | Déplacé **entier** dans `cgarchi` | 2026-10-22 |
| D3 | `metaballs.cpp` (démo de Takayama 2024, code mort compilé) | Supprimé | 2026-10-22 |
| D4 | Catalogue paramétrique | `cgmesh` garde ses 25 formes ; `cgarchi` expose `GothicParametricShapes()` ; les deux sont réunis dans `cggraph_nodes`, et `maker` passe par cette recherche réunie. Pas d'auto-enregistrement statique. | 2026-10-22 |
| D5 | Adaptateurs (nœud `gothic_window` de cggraph, API gothique de `wasm_api.cpp`) | Restent dans leur couche (`cggraph_nodes`, `maker`) | 2026-10-22 |
| D6 | Ordre des déplacements | Partie `cgmesh` (E2) **avant** partie `cgmath` (E3), sinon cycle transitoire `cgmesh → cgarchi → cgmesh` | 2026-10-22 |
| D8 | Collision de noms entre `cgmath/architecture_gothic*` et `cgarchi/architecture_gothic*` (venu de `cgmesh`) | Les fichiers venant de `cgmath` sont renommés `architecture_gothic_2d.{h,cpp}` et `architecture_gothic_2d_io.{h,cpp}` ; les schémas vont dans `src/cgarchi/schemas/` | 2026-10-22 |
| D7 | Espace de noms | `namespace cgarchi` à l'étape séparée E4, pas pendant un déplacement de fichiers | 2026-10-22 |

## 3. Périmètre

### Déplacé dans `cgarchi`

- `src/cgmath/architecture_gothic.{h,cpp}`, `architecture_gothic_io.{h,cpp}`,
  `gothic-tracery.schema`, `gothic-window-v2.schema`
- `src/cgmesh/architecture_gothic.{h,cpp}`, une fois les primitives génériques sorties (E1a)
- `src/cgmesh/architecture_gothic_region.{h,cpp}`
- `src/cgmesh/parameterized_gothic.{h,cpp}`, `parameterized_gothic_json.cpp`
- `src/cgmesh/gothic_print_json.{h,cpp}`, extrait de `print_validation_json.cpp` (E1b)
- `src/cgmesh/gothic_parametric_catalog.{h,cpp}` : `GothicParametricShapes()` (E1c)
- `src/cgmesh/bar_fusion.{h,cpp}`, `bar_fusion_parallel.{h,cpp}` (D1)
- `src/cgmesh/surface_architecture.{h,cpp}` (D2)

### Reste en place

- Primitives génériques : `Circle`, `Ellipse`, `Arc` (`cgmath/geometry.h`), `Profile2D`,
  `print_validation`, `contour_validation`, `mesh_validation`, `polygon2*`, `contour_ops`,
  `extrude_contours`, Clipper2.
- Primitives sorties de `architecture_gothic.cpp` en E1a : `tessellateToMesh`, `extrudeToMesh`,
  `sweepProfileAlongArc(s)`, `appendMesh`, profils rectangulaire, chanfrein et cavet.
- Données de test `test/data/gothic-*` (leurs chemins sont codés en dur dans `maker` et dans les
  tests).

## 4. Coutures à défaire dans `cgmesh` [V]

1. `parametric_catalog.cpp:4,45-46` inclut `parameterized_gothic.h` et enregistre deux formes
   gothiques.
2. `print_validation_json.cpp:18` inclut `parameterized_gothic.h`. `GothicPrintVerdictToJson`
   utilise des fonctions de l'espace de noms anonyme de ce fichier.
3. `cgmesh.h:28` inclut `surface_architecture.h`.

Piège : `maker/wasm_api.cpp:173` fait `if (!FindParametricShape(name)) continue;`. Si `maker`
n'interroge que le catalogue de `cgmesh`, les deux formes gothiques disparaissent sans erreur.

## 5. Étapes

Chaque étape compile et passe `TU` et `TU_faults`.

| Étape | Contenu | Critère d'acceptation | Statut |
|---|---|---|---|
| E0 | Supprimer `metaballs.cpp` (D3) | Le nombre de tests ne change pas | fait et revu |
| E1a | Sortir les primitives génériques de `architecture_gothic.{h,cpp}` vers `polygon_extrude.{h,cpp}` et `profile_sweep.{h,cpp}`. `architecture_gothic.h` inclut les deux en-têtes. | Même nombre de tests ; ensemble des symboles de `cgmesh.lib` inchangé | fait et revu |
| E1b | `PrintReportToJson(PrintReportEnvelope, PrintabilityReport)` générique ; `GothicPrintVerdictToJson` va dans `gothic_print_json.{h,cpp}` | `print_validation_json.cpp` n'inclut plus de fichier gothique ; JSON de `tu_cgmesh_print_validation_json` identique à l'octet près | fait et revu |
| E1c | Catalogue scindé (D4) : `GothicParametricShapes()`, `cggraph_nodes::CatalogShapes / FindCatalogShape / MakeCatalogShape` ; entrée inconnue de `wasm_api.cpp` signalée sur `std::cerr` | 25 entrées dans `cgmesh`, 27 dans la recherche réunie ; toutes les liaisons de `tu_cggraph_nodes_shapes` se résolvent | fait et revu ; lien WASM vérifié (R1 levée) |
| E2 | `src/cgarchi` créé ; partie `cgmesh` déplacée (`mv`, l'utilisateur gère l'index) ; CMake de la racine, de `cggraph/nodes`, de `maker` et de `test` ; **9** entrées retirées de la liste WASM de `cgmesh` ; inclusions en `<cgarchi/…>`, y compris dans `shape_catalog.cpp`, `wasm_api.cpp`, `tu_cgmesh_solidify.cpp` et `tu_cgmesh_print_validation_json.cpp` ; `cgmesh.h:28` retiré | `ls src/cgmesh` ne contient ni `gothic`, ni `bar_fusion`, ni `surface_architecture` ; même nombre de tests | fait et revu (WARNING, 4 commentaires corrigés) ; GCC non vérifié (R4) |
| E3 | Partie `cgmath` déplacée ; `extern` retiré de `src/cgmath/CMakeLists.txt:13` | `grep -rl nlohmann src/cgmath` → 0 | fait et revu (WARNING, 2 commentaires corrigés) ; GCC non vérifié (R4) |
| E4 | `namespace cgarchi`, tests renommés en `tu_cgarchi_*` (dont `tu_cgmath_architecture_gothic*` et `tu_cgmesh_*gothic*`), nœud déplacé dans `cggraph/nodes/archi/` | Symboles exportés = ancien ensemble préfixé ; même nombre de tests | fait et revu (APPROVE, aucun constat) |
| E5 | Docs : `debt_cgarchi.md`, `debt_cgmath.md` §1.5 et anciens chemins (l. 126, 137, 208, 272-279), `src/cgmesh/docs/gothic_thesis_implementation_plan.md` et `gothic_ui.md` (chemins `src/cgmath/…`), `debt_cgmesh.md:408-410` (`metaballs.cpp`), `CLAUDE.md`, commentaires de `geometry.h:110,217` ; `src/cgmesh/docs/gothic_*.md` → `src/cgarchi/docs/` | Relecture | fait (docs dans `src/cgarchi/docs/`, `debt_cgarchi.md` créé, renvois dans les autres `debt_*.md`) |

Détecteur de l'étape E1 :

    grep -lE '#include.*(gothic|bar_fusion|surface_architecture)' src/cgmesh/*.h src/cgmesh/*.cpp

Une fois exclus les fichiers de §3, il doit trouver exactement `parametric_catalog.cpp`,
`print_validation_json.cpp` et `cgmesh.h` **avant** E1 ; c'est sa validation sur un cas positif.
Après E1, il doit trouver uniquement `cgmesh.h`, qui disparaît en E2. Le motif porte sur
`gothic` et non sur une liste de noms d'en-têtes, pour couvrir aussi les fichiers créés en E1.

## 6. Réserves

- **R1** ✅ **levée le 2026-10-22** (lien `maker-wasm` OK ; sous Node, `listShapes("gothic")` → `["Gothic Window","Gothic Block"]`, 25 formes paramétriques, 27 au total, 0 message sur stderr ; cas négatifs : catégorie inconnue → `[]`, nom inconnu → `-1`). Historique : aucune CI ne construit `maker` en WebAssembly. Un build natif réussi ne prouve pas que
  la liaison WebAssembly réussit. Pour lever la réserve : construire `maker-wasm` avec emsdk, puis
  vérifier que `listShapes("gothic")` renvoie 2 entrées sous Node. emsdk est présent
  (`C:/home/dev/extern/emsdk`) et `build/maker-wasm` est configuré. Le lien réécrit
  `maker/web/maker.{js,wasm}`, qui sont suivis par git.
- **R2** : la taille d'E1a (environ 430 lignes) est estimée à partir des bornes des définitions,
  pas mesurée.
- **R3** : `src/cgmesh/docs/plan.md` (microarchitecture de Segall 2025, non suivi par git) prévoit
  de nouveaux fichiers `gothic_microarchitecture*` dans `cgmesh`. Leur cible devient `cgarchi`.
- **R4** : sous UNIX, `cgmesh` compile avec `-fpermissive` (`add_compile_options` limité à son
  répertoire), et `cgarchi` n'en hérite pas. `surface_architecture.cpp`, ancien code en `float**`,
  n'a jamais été compilé par GCC sans ce drapeau. clang (`em++`) l'accepte ; GCC reste à vérifier.

## 7. Journal de prédictions

| Étape | Prédiction | Instrument | Résultat | Écart | Cause |
|---|---|---|---|---|---|
| Référence | — | `TU --gtest_list_tests`, exécution | TU 2324 tests (2312 exécutés + 12 désactivés), 0 échec ; TU_faults 5/5 | — | — |
| E1 détecteur | 3 sites avant, 1 après (`cgmesh.h`) | `grep` de §5 | Avant : `cgmesh.h`, `parametric_catalog.cpp`, `print_validation_json.cpp`. Après E1b : `cgmesh.h`, `parametric_catalog.cpp`. Après E1c : `cgmesh.h` | aucun | — |
| E0 | Nombre de tests inchangé | gtest ; `dumpbin /linkermember:1` | TU 2324, TU_faults 5 ; symboles publics de `cgmesh.lib` 27156 → 26769 (232 `metaballs_demo::*`, le reste des instanciations std) | aucun | — |
| E1a | Ensemble des symboles inchangé ; environ 430 lignes déplacées | `dumpbin /linkermember:1`, `git diff --numstat` | 26769 → 26769, `diff` vide (instrument validé par la différence d'E0) ; −422 lignes sorties, 448 lignes dans les nouveaux fichiers | négligeable | inclusions et commentaires des nouveaux fichiers |
| E1b | JSON identique à l'octet près | capture temporaire avant/après, `cmp` | 5 documents, 22096 octets de chaque côté, aucune différence | aucun | — |
| E1c | 25 + 2 = 27 ; `maker` rend 2 formes gothiques | gtest ; Node (R1) | 25 / 2 / 27 ; TU 2325 (+1 test), TU_faults 5. `wasm_api.cpp` passe `em++ -fsyntax-only` (contrôle positif : un appel muté échoue) ; lien et Node non mesurés | lien WASM non mesuré | build WASM refusé par l'utilisateur le 2026-10-22 |
| Revue E0–E1c | — | `code-reviewer` | WARNING : 0 critique, 0 grave, 2 moyens, 5 mineurs. Moyens et 3 mineurs corrigés ; tests `PrintReportToJson` ajoutés (inversion `allowed`/`strict` détectée : 1 échec, puis code restauré). TU 2327 (2315 exécutés + 12 désactivés), 0 échec ; TU_faults 5/5 | — | — |
| E2 | Environ 30 inclusions à réécrire, toutes en erreur de compilation franche | compilateur, `grep` | 44 réécrites (18 internes à `cgarchi`, 26 externes) + 1 ajoutée (`tu_cgmesh_architecture.cpp`, qui passait par le parapluie `cgmesh.h`) ; détecteur `src/cgmesh` → 0 ; TU 2327 / TU_faults 5, 0 échec ; `em++ -fsyntax-only` OK sur les 9 `.cpp` de `cgarchi`, sur 3 de `cggraph` et sur `wasm_api.cpp` | +14 inclusions | la prédiction ne comptait pas les inclusions internes à `cgarchi` ; « erreur franche » non observée, réécriture faite avant compilation |
| E3 | `cgmath` perd environ 2 000 lignes et nlohmann | `wc -l`, `grep` | −2022 lignes de C++ (13356 → 11334) et 852 lignes de schémas ; `grep nlohmann src/cgmath` → 0 ; `gothic` ne reste que dans 2 commentaires légitimes de `geometry.h` ; TU 2327 / TU_faults 5, 0 échec ; tests sans schéma au nouveau chemin → 2 échecs (cas négatif) ; `em++ -fsyntax-only` OK sur 15 unités | aucun | — |
| E4a | Même nombre de tests ; liste de tests identique aux préfixes près | `--gtest_list_tests`, diff normalisé | 12 fichiers renommés `tu_cgarchi_*` (ceux de `cgmath` suffixés `_2d` pour éviter une collision) ; diff normalisé vide, 2327 / 5 (instrument : 892 lignes sans normalisation) | aucun | — |
| E4b | Symboles de `cgarchi.lib` = ancien ensemble préfixé `cgarchi::` | `dumpbin /linkermember:1` + `undname` | 7488 / 7488 ; 10 lignes d'écart, toutes des lambdas dans `std::variant` (nom haché selon le contexte) ; 0 fonction restée globale (contrôle : 53 avant) ; défaut trouvé par `em++` (ouverture du namespace sous `#ifndef __EMSCRIPTEN__` dans `bar_fusion_parallel.cpp`) et corrigé | négligeable | hachage des lambdas |
| E4c | Identifiants sérialisés inchangés ; graphe ancien chargé | gtest | `gothic_window` dans `cggraph/nodes/archi/` ; `shape.gothic.window` et `shape.gothic.block` inchangés ; `graph_documents_written_before_the_print_check_compute_as_before` passe ; TU 2327 / 5 | aucun | — |
| Lien WASM (R1) | `maker` se lie ; la page gothique liste 2 formes | `cmake --build build/maker-wasm --target maker` ; harnais Node (scratchpad `r1_check.mjs`) | build OK (2 avertissements ImGui) ; `["Gothic Window","Gothic Block"]` ; 25 + 2 = 27 ; maillages non vides (y compris depuis `gothic-window-instance.json`) ; 0 message stderr ; `maker.wasm` 2 949 902 → 2 960 029 octets | détecteur stderr non validé sur un cas positif | il faudrait modifier le dépôt pour en produire un |

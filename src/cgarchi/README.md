# cgarchi

Bibliothèque statique d'architecture (`cg::cgarchi`), C++17, au-dessus de `cgmesh`. Tout son code est dans
`namespace cgarchi`.

## Responsabilité

Ce qui appartient au module : la géométrie spécialisée, les composants architecturaux, leur construction, leurs
catalogues et leur sérialisation.

- **gothique** : remplage 2D (`architecture_gothic_2d`), région de pierre (`architecture_gothic_region`), fenêtre
  paramétrée et son JSON v2 (`parameterized_gothic*`, `schemas/`), fusion des barres de moulure (`bar_fusion*`),
  contrôle d'impression (`CheckGothicPrint`, `gothic_print_json`), microarchitecture (`gothic_microarchitecture*`) ;
- **classique** : les cinq ordres de Vignole (`classical_*`, `parameterized_classical`), dont les données
  `data/cinq_ordres.json` sont embarquées dans le binaire à la construction (`CMakeLists.txt:19-33`) ;
- **démonstrations** : `surface_architecture` (arcs échantillonnés, balayage de moulure, `Rosace`, `ArcBrise`, blocs) ;
- **catalogues** : `GothicParametricShapes()` et `ClassicalParametricShapes()`. Une forme paramétrique gothique ou
  classique s'ajoute à l'une de ces deux listes ; la réunion avec celle de `cgmesh` se lit chez le consommateur,
  `cggraph_nodes::CatalogShapes()` (`src/cggraph/nodes/shapes/shape_catalog.h`).

Ce qui n'y appartient pas : les primitives génériques déjà offertes par `cgmesh` et `cgmath` (`appendMesh`,
`Mesh::ComputeNormals`, tessellation, extrusion), que `cgarchi` réutilise. Quelques outils génériques y vivent
pourtant parce qu'ils n'ont qu'un consommateur : `bar_fusion`, `GridIsoExtractor`, `WindingNumber`, `PlanArc`,
`MakeTriangleMesh`, les prédicats de polygone de `classical_volute`. Règle : un outil générique descend dans `cgmesh`
(ou `cgmath` pour le 2D) quand un **second** consommateur apparaît, pas avant (`debt_cgarchi.md` §2.4).

Le sens des dépendances est unique : aucun fichier de `cgmath`, `cgimg` ou `cgmesh` n'inclut un en-tête de
`cgarchi`. Toutes les sources compilent aussi sous Emscripten (`CMakeLists.txt:8-9`) : `maker` lie la cible.

## Dépendances

| Nature | Dépendance | Activation |
|---|---|---|
| interne | `cgmesh`, lien `PUBLIC` (`CMakeLists.txt:56`), donc `cgimg` et `cgmath` | toujours |
| tierce | nlohmann/json, Clipper2, threads : reçus de `cgmesh`, non déclarés ici (`CMakeLists.txt:52-55`) | toujours |
| génération | `embed_file.cmake` produit `classical_orders_data.cpp` depuis `data/cinq_ordres.json` | toujours |

Aucune option `ENABLE_*` ne touche ce module. Les en-têtes de `cgarchi` s'incluent par `<cgarchi/x.h>` ; ceux de
`cgmesh`, par leur nom seul (`CMakeLists.txt:35-38, 48-50`).

## Usage minimal

Repris de `test/tu_cgarchi_classical_column.cpp`, test
`each_order_builds_base_shaft_capital_stacked_to_the_column_height` (l. 44-48) :

```cpp
#include <cgarchi/parameterized_classical.h>

cgarchi::ClassicalColumnSettings s;
s.order = 0;                                  // classical::Order::Tuscan
s.module = 2.5f;                              // demi-diamètre inférieur du fût, en unités de scène
cgarchi::classical::ColumnProfile profile;
const std::unique_ptr<Mesh> m = cgarchi::BuildClassicalColumnMesh (s, &profile);
// axe Z, pied en z = 0, hauteur order.general.column_M * s.module
```

## Tests

Fichiers `test/tu_cgarchi_*.cpp` (exécutable `TU`) et `test/faults/tu_cgarchi_gothic_print_faults.cpp`
(`TU_faults`, absent sous `CG_ENABLE_SANITIZERS`). Toutes les suites portent le préfixe `TEST_cgarchi_`, y compris les
tests paramétrés de `tu_cgarchi_gothic_baseline.cpp` (préfixe d'instanciation, l. 883 et 904).

```powershell
ctest --test-dir build/<preset> -C Release -R "TEST_cgarchi_"
ctest --test-dir build/<preset> -C Release -R "TEST_cgarchi_" -LE slow   # sans les tests longs
```

Les tests lisent `test/data` et `src/cgarchi/schemas`, que le build copie dans le répertoire d'exécution
(`test/CMakeLists.txt`, `TU_RUN_DIR`). Quatre tests nommés `tu_cgmesh_*` exercent aussi `cgarchi` (voir
`src/cgmesh/README.md`).

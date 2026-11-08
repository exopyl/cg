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
- **temple** : implantation des axes d'un péristyle, sans maillage, en unités de l'appelant (`classical_peristyle`) ;
- **dorique grec** : une colonne sans base, à entasis, cannelures à arête vive et chapiteau à gorgerin cannelé et rainure, annelets en gradins, échine en droite et lèvre, et abaque, en unités de l'appelant ; réglages en valeur et fonctions pures, sans adaptateur ni catalogue (`classical_greek_doric_column`) ;
- **krépis** : degrés rectangulaires sous le stylobate, placés puis maillés par `extrudeToMesh`, en unités de l'appelant ; réglages en valeur et fonctions pures, sans adaptateur ni catalogue (`classical_krepis`) ;
- **assemblage du temple** : krépis et péristyle de colonnes doriques grecques, deux jeux de réglages de colonne (courante, angle), contraction d'angle donnée, placements, adresses, refus des contacts et maillage assemblé en un bloc, en unités de l'appelant ; réglages en valeur et fonctions pures, sans adaptateur ni catalogue (`classical_temple`) ;
- **entablement grec** : partitions 1D (`classical_partition`) ; anneau balayé le long du rectangle des axes, section en bandes à faces verticales ou inclinées, frise monotriglyphe (un triglyphe sur chaque axe et au milieu de chaque travée, demi-métope d'angle provisoire), triglyphes en prismes posés au nu des métopes ; optionnel dans le temple (`classical_greek_entablature`) ;
- **document du monument** : le temple d'Héphaïstos, `data/temple_hephaistos.json` (`monument/v6`, schéma `schemas/monument-v6.schema`), valeurs du README de l'utilisateur avec unité, statut, sources et lieu, entablement compris, écrit par `data/temple_hephaistos_from_readme.py` (lancé à la main, jamais par la construction ; `--check` compare le JSON à la dérivation du README) et embarqué à la construction (`CMakeLists.txt:35-53`) ; lecteur strict à exception (`ParseMonument`, `HephaistosMonument`) et conversion vers `KrepisSettings`, `PeristyleSettings`, les deux `GreekDoricColumnSettings`, `GreekEntablatureSettings` (`MonumentEntablatureSettings`) et `TempleSettings` (`MonumentTempleSettings`, temple du README), façade est du README en façade du temple (`classical_monument`) ;
- **démonstrations** : `surface_architecture` (arcs échantillonnés, balayage de moulure, `Rosace`, `ArcBrise`, blocs) ;
- **catalogues** : `GothicParametricShapes()` et `ClassicalParametricShapes()`. Une forme paramétrique gothique ou
  classique s'ajoute à l'une de ces deux listes ; la réunion avec celle de `cgmesh` se lit chez le consommateur,
  `cggraph_nodes::CatalogShapes()` (`src/cggraph/nodes/shapes/shape_catalog.h`). Chaque entrée porte aussi son
  identifiant, qui est le type de nœud des documents `cggraph` et ne change jamais, et sa catégorie (`gothic`,
  `classical`), qui choisit la page de `maker`. `cggraph` et `maker` les lisent là, sans copie ;
- **sérialisation** : le lecteur JSON strict commun (`json_reader`). Il refuse les champs inconnus, contrôle les
  types et les domaines, refuse les clés en double (`ParseRejectingDuplicateKeys`, appelé par les trois lecteurs),
  et nomme chaque refus par le chemin JSON du champ. Il applique la convention
  `"schema": "nom/vN"` (`ReadSchema`, `ParseSchemaId`). La fenêtre gothique et la microarchitecture en dérivent
  leur `Reader`. La colonne classique a son document `"classical-column/v1"` (`parameterized_classical_json.h`,
  schéma `schemas/classical-column-v1.schema`) : `LoadClassicalColumnFromJson` et `ExportClassicalColumnJson`,
  fonctions pures sur `ClassicalColumnSettings`, ordre écrit par sa clé (`classical::OrderKey`), tous les champs
  requis, clés en double refusées (`ParseRejectingDuplicateKeys`), écriture canonique relue à l'identique.

Ce qui n'y appartient pas : les primitives génériques déjà offertes par `cgmesh` et `cgmath` (`appendMesh`,
`Mesh::ComputeNormals`, tessellation, extrusion), que `cgarchi` réutilise. Quelques outils génériques y vivent
pourtant parce qu'ils n'ont qu'un consommateur : `bar_fusion`, `GridIsoExtractor`, `WindingNumber`, `PlanArc`,
`MakeTriangleMesh`, les prédicats de polygone de `classical_volute`. Règle : un outil générique descend dans `cgmesh`
(ou `cgmath` pour le 2D) quand un **second** consommateur apparaît, pas avant (`debt_cgarchi.md` §2.4).

Le sens des dépendances est unique : aucun fichier de `cgmath`, `cgimg` ou `cgmesh` n'inclut un en-tête de
`cgarchi`. Toutes les sources compilent aussi sous Emscripten (`CMakeLists.txt:8-9`) : `maker` lie la cible.

## Convention : réglages, fonctions pures, adaptateur

Décision D2-1 du plan 2 (`plan_implementation_02_revision.md` §5.1). Commun à la fenêtre gothique
(`GothicWindowSettings`, `parameterized_gothic.h`) et à la colonne classique (`ClassicalColumnSettings`,
`parameterized_classical.h`) :

- **réglages en valeur** : une structure copiable, `*Settings`, seul exemplaire des valeurs du composant ;
- **adaptateur `ParameterizedMesh`** : il tient un `*Settings m_settings`, lie ses `Parameter` aux champs de
  `m_settings` sans recopie, et expose `Settings ()` et `SetSettings ()` ; `SetSettings` remplace les réglages sans
  régénérer ;
- **construction** séparée de `IParameterized`, à partir des réglages.

Propre à chacun, le bornage :

- **classique** : `Regenerate` borne `m_settings` en place par `ClampClassicalColumnSettings`, partagée avec la
  colonnade, puis appelle la fonction pure `BuildClassicalColumnMesh`. Celle-ci ne borne rien. Elle ne vérifie qu'un
  domaine, `order`, et lève `std::invalid_argument` avant toute écriture dans ses sorties. `module` et les segments
  ne sont pas vérifiés (`debt_cgarchi.md` §2.3) ;
- **gothique** : les réglages ne sont jamais modifiés. Le bornage porte sur des copies (`prepareBay`, `const`), chaque
  borne appliquée est un avertissement `parameter_clamped` du rapport, et une valeur non finie est une erreur
  `invalid_parameter`.

Inspection sans maillage, pour le classique : `ComputeClassicalColumnLevels (settings)` rend les cotes z
(`ClassicalColumnLevels` : pied, sommet du piédestal s'il est demandé, pied et sommet du fût, pied et sommet du
chapiteau, sommet, pied de l'entablement s'il est demandé) de la colonne que `BuildClassicalColumnMesh` construirait pour les mêmes réglages. Elle
construit le profil seul, sans tour ni volutes, avec les mêmes options et la même échelle que le maillage. Chaque
cote est donc celle d'un anneau du maillage (test `each_level_is_the_height_of_a_vertex_of_the_column_mesh`). Elle a le domaine
de `BuildClassicalColumnMesh` : elle lève `std::invalid_argument` pour un `order` hors domaine, et ne borne ni ne
vérifie `module` ni les segments. Exclusion : les coussinets ioniques sont hors profil. Leurs sommets ne sont pas
sur des anneaux, et les cotes ne couvrent pas l'emprise horizontale de l'ionique à volutes ; en z, ces sommets
restent dans [`foot`, `top`] (même test). Coût : environ 1/50 à 1/5 de celui du maillage selon l'ordre
(`baseline_performance.md` §8.9).

Les noms, l'ordre et les bornes des `Parameter` sont des clés des documents `cggraph` et des liens profonds de `maker` :
ils ne changent pas. Seules les formes classiques ont un test qui fige cette surface
(`the_parameter_surface_of_the_classical_shapes_is_pinned`). Ne suivent pas encore la convention : la colonnade
(`m_settings` sans `Settings ()` ni `SetSettings ()`) et le bloc gothique (réglages en membres).

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

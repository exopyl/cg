# cgmesh

Bibliothèque statique de maillages et de géométrie 2D/3D (`cgmesh`), C++17, au-dessus de `cgimg` et `cgmath`.

## Responsabilité

Cœur du module : `Mesh` et ses opérations (transformations, normales, triangulation, demi-arêtes, octree, lancer de
rayons), `Polygon2` et sa tessellation, profils 2D (`Profile2D`), extrusion (`polygon_extrude.h`,
`extrude_profiled.h`), balayage (`profile_sweep.h`), opérations sur contours (Clipper2), tranchage, validation de
maillages, de contours et d'impression, formes paramétriques génériques (`ParametricShapes()`). Le tour de révolution
cité par le plan 1 (§7.2) n'y est pas : le seul existant est celui des ordres classiques, dans `cgarchi`
(`classical_lathe.h`).

Le contenu réel déborde ce cœur ; ces familles y vivent aussi (regroupement par nom de fichier, non exhaustif) :
entrées-sorties de formats (`mesh_io*`, `vmeshes_io*`, `import_svg`, `import_scad`), reconstruction 3D à partir
d'images (`recon_*`), images vers reliefs et contours (`image_relief`, `image_vectorization`), rendu (`raytracer`,
`NPR*`, matériaux), voxels, descripteurs de forme et estimateurs de courbure, audio, lancement de processus externes.

Ce qui n'y appartient pas :

- **aucun code gothique ni architectural** : il vit dans `cgarchi`, que `cgmesh` n'inclut pas. Dans le code (hors
  commentaires), « gothi » ne se trouve que dans `MeshAlgoThickness` (« goThi », `thickness.h:25`). Des commentaires
  nomment encore des fonctions de `cgarchi` (`profile2d.h:11-17`) ;
- exception connue : `extrudeProfiledToMesh` (`profile2d.cpp:146`) porte des seuils réglés sur le remplage gothique
  (`profile2d.cpp:201-202`) et n'a qu'un appelant de production, `cgarchi/parameterized_gothic.cpp:651`. Il reste ici
  par décision (dette, `debt_cgmesh.md` §2.5).

Sous Emscripten, seul un sous-ensemble de sources est compilé (liste `SRC_FILES`, `CMakeLists.txt:7-145`).

## Dépendances

| Nature | Dépendance | Activation |
|---|---|---|
| interne | `cgimg` (donc `cgmath`), lien sans mot-clé (`CMakeLists.txt:181`) | toujours |
| tierce, compilée dans `cgmesh` | glutess (`extern/glutess`, sauf `main.c`), Clipper2 (moteur et offset) | toujours (`CMakeLists.txt:151-172`) |
| tierce, en-têtes seuls | nlohmann/json, tinygltf, stb_image, nanosvg, geodesic (`extern/`, PUBLIC SYSTEM, l. 180) | toujours |
| système | threads (`Threads::Threads`, l. 238-239) | toujours |
| zlib | compilée depuis `extern/zlib-1.3.2` sous Windows, zlib du système ailleurs (l. 424-464) | `ENABLE_ZLIB`, ON par défaut |
| OpenNURBS (`.3dm`) | binaire, `EXTERN_OPENNURBS` (l. 241-263) | `ENABLE_OPENNURBS`, ON par défaut |
| lib3mf (`.3mf`) | binaire, `EXTERN_LIB3MF` (l. 277-301) | `ENABLE_LIB3MF`, OFF par défaut |
| OCCT (STEP, IGES) | binaire, `EXTERN_OCCT` sous Windows, `find_package` ailleurs (l. 303-384) | `ENABLE_OCCT`, OFF par défaut |
| ONNX Runtime (profondeur) | binaire, `EXTERN_ONNXRUNTIME` (l. 386-406) | `ENABLE_ONNX`, OFF par défaut |
| PoissonRecon | sources `extern/PoissonRecon-18.76` (l. 408-422) | `ENABLE_POISSON`, OFF par défaut |

Chaque option active définit une macro PUBLIC `CG_HAS_*`. Les défauts sont ceux du `CMakeLists.txt` racine ; les
presets les changent. `ENABLE_CGRE` ajoute un lien à la cible `cgmesh` depuis `src/cgre/CMakeLists.txt:70`.
OpenSCAD est un exécutable externe lancé à l'exécution (`import_scad.h`), jamais lié.

## Usage minimal

Repris de `test/tu_cgmesh_polygon_extrude_edge_cases.cpp` (`UnitSquare`, l. 27-31 ; `Validate`, l. 35-40 ; test
`extruded_square_is_a_closed_outward_shell_of_its_volume`, l. 60-71) :

```cpp
#include "mesh.h"
#include "mesh_validation.h"
#include "polygon2.h"
#include "polygon_extrude.h"

Polygon2 square;
float pts[8] = {0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f};   // carré unité, sens trigonométrique
square.add_contour (0, 4, pts);

Mesh m;
extrudeToMesh (square, m, -1., 2.);                         // prisme de z = -1 à z = 2

MeshValidationOptions options;
options.weldTolerance = 1e-6;
const MeshValidationReport r = ValidateMesh (m, options);   // r.IsValid (), r.signedVolume = 3
```

Les en-têtes de `cgmesh` s'incluent par leur nom seul (répertoire PUBLIC `src/cgmesh`, `CMakeLists.txt:175`).

## Tests

Fichiers `test/tu_cgmesh_*.cpp` (exécutable `TU`) et `test/faults/tu_cgmesh_*.cpp` (exécutable `TU_faults`, absent
sous `CG_ENABLE_SANITIZERS`), plus `TU_glutess_main_probe`.

```powershell
ctest --test-dir build/<preset> -C Release -R "TEST_cgmesh_"
ctest --test-dir build/<preset> -C Release -R "TEST_cgmesh_" -LE slow   # sans les tests longs
```

- Le motif ne capture pas les suites de `tu_cgmesh_*` nommées autrement : `ExternalProcess`, `GeodesicWrapperTest`,
  `ImportScad`, `ScadTestSupport`, `OffReader`, `TensorSaddle`, `TensorSaddle_Steiner`, `TEST_bundle`,
  `TEST_bundle_camera`, `TEST_mesh_slicing`, `TEST_regions_faces`, `TEST_regions_vertices`, `TEST_slicer`,
  `TU_cgmesh_solidify`, `TEST_cgmath_raytracer`.
- Les tests liés à une option (OpenNURBS, OCCT, lib3mf, zlib, Poisson) n'existent que si l'option est active.
- Quatre fichiers de tests de `cgmesh` incluent `cgarchi` (`tu_cgmesh_parameterized_shapes.cpp:15`,
  `tu_cgmesh_print_validation_json.cpp:14,17`, `tu_cgmesh_solidify.cpp:8`,
  `faults/tu_cgmesh_implicit_surface_faults.cpp:12`).
